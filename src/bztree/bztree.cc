#include "bztree.h"
#include "common/allocator_internal.h"

#define POOL_SIZE 1000
#define POOL_THREADS 10

#define GLOBAL_EPOCH_OFFSET_BIT (1 << 27)

namespace pmwcas {

BzTree::BzTree() {
  assert(epoch.Initialize().ok());
  assert(garbage.Initialize(&epoch).ok());

#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  pop = allocator->GetPool();

  const struct BzPMDKMetadata *metadata = this->get_metadata();

  if (metadata == nullptr) {
    if (DEBUG_PRINT_ACTIONS) printf("--- init new\n");
    // new bztree, who this
    TOID(struct BzPMDKMetadata) newmetadata_oid;
    POBJ_ZNEW(pop, &newmetadata_oid, struct BzPMDKMetadata);
    struct BzPMDKMetadata *newmetadata = D_RW(newmetadata_oid);

    // new root node is a leaf node, so we want the entire node
    POBJ_ZNEW(pop, &newmetadata->root_node, struct Node);
    newmetadata->height = 1;
    global_epoch = newmetadata->global_epoch = 0;

    // we also need a new descriptor pool, this is safe though
    TOID(DescriptorPool) desc_pool_oid;
    POBJ_ZNEW(pop, &desc_pool_oid, DescriptorPool);
    desc_pool = D_RW(desc_pool_oid);
    new(desc_pool) DescriptorPool(POOL_SIZE, POOL_THREADS);

    // install the new root and descriptor pool ptr, also, test pmwcas on init
    struct BzPMDKRootObj *rootobj = D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj));
    rootobj->metadata = newmetadata_oid;
    rootobj->desc_pool = desc_pool_oid;
  } else {
    if (DEBUG_PRINT_ACTIONS) printf("--- init existing\n");
    // grab the descriptor pool ptr
    // todo(persistence): check if we have to initialize? DescriptorPool has a third param for existing vm addr...
    desc_pool = D_RW(D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj))->desc_pool);
    // todo(persistence): after verifying desc_pool works: increment the global epoch
    // also throw if global epoch cannot fit in 28 bits, because we borrow offset for that
    global_epoch = 1;
    printf("did you forget to rm the pool (replace me when persistence is implemented)");
    // assert(0);
  }
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
}

BzTree::~BzTree() {
  Thread::ClearRegistry(true);
}

bool BzTree::insert(const std::string key, const std::string value) {
  if (DEBUG_PRINT_ACTIONS) printf("--- insert %s %s\n", key.c_str(), value.c_str());
  size_t space_required = sizeof(struct NodeMetadata) + key.length() + 1 + value.length() + 1;
  // exit early if it is too large for any node
  if (space_required > BZTREE_MIN_FREE_SPACE) return false;

  assert(epoch.Protect().ok());
  TOID(struct Node) leaf_oid = find_leaf(key, true);
  struct Node *leaf = D_RW(leaf_oid);
  struct NodeMetadata *nmd = reinterpret_cast<struct NodeMetadata*>(leaf->body);

  // check for existing value
  // this first pass is only opportunistic, it's not to formally check for existing value
  // it catches the common bad insertion case though
  bool recheck = false;
  // todo(optimization): binary search sorted keys
  for (uint16_t i=0; i<leaf->header.status_word.record_count; i++) {
    // any not-visible ones potentially are key conflicts in the middle of insertion, if in the same epoch
    if (!nmd[i].visible && nmd[i].offset == (global_epoch | GLOBAL_EPOCH_OFFSET_BIT)) recheck = true;
    if (nmd[i].visible && strcmp(&leaf->body[nmd[i].offset], key.c_str()) == 0) {
      // fail because we found one that's already the same key
      assert(epoch.Unprotect().ok());
      return false;
    }
  }

  // reserve space for metadata and key value entry
  while (1) {
    // set up pmwcas to allocate space on the node
    // copy status word
    struct NodeHeaderStatusWord sw_old = leaf->header.status_word;
    struct NodeHeaderStatusWord sw = sw_old;
    if (sw.block_size + space_required > sizeof(struct Node) - sizeof(struct NodeHeader) -
        sw.record_count * sizeof(struct NodeMetadata)) {

      // too large to fit - something went wrong since we never should've seen this from find_leaf's SMOs
      assert(epoch.Unprotect().ok());
      assert(false);
      return false;
    }
    sw.block_size += key.length() + 1 + value.length() + 1;
    sw.record_count += 1;

    // copy node metadata
    struct NodeMetadata md_old = nmd[sw_old.record_count];
    struct NodeMetadata md = md_old;
    assert(md.visible == 0);
    md.offset = (global_epoch | GLOBAL_EPOCH_OFFSET_BIT);

    // pmwcas to allocate space on the node
    auto *desc = desc_pool->AllocateDescriptor();
    assert(desc);
    desc->AddEntry((uint64_t*)&leaf->header.status_word, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
    desc->AddEntry((uint64_t*)&nmd[sw_old.record_count], *(uint64_t*)&md_old, *(uint64_t*)&md);
    if (!desc->MwCAS()) {
      // collision, so another thread tried to allocate here, so retry
      // set recheck flag because the other allocation may be for a conflicting key
      recheck = true;
      continue;
    }
    break;
  }

  // set up pmwcas to make record visible, also check and ensure the frozen bit
  // status word is not modified, only to ensure frozen bit
  struct NodeHeaderStatusWord sw = leaf->header.status_word;

  // now we can basically safely work, copy the key and value in
  struct NodeMetadata md_old = nmd[sw.record_count-1];
  struct NodeMetadata md = md_old;
  md.offset = sizeof(leaf->body) - sw.block_size;
  md.key_len = key.length() + 1;
  md.total_len = key.length() + 1 + value.length() + 1;
  pmemobj_memcpy_persist(pop, &leaf->body[md.offset], key.c_str(), key.length() + 1);
  pmemobj_memcpy_persist(pop, &leaf->body[md.offset + md.key_len], value.c_str(), value.length() + 1);

  if (sw.frozen) {
    // must retry entire thing (including traversal) since this is frozen
    assert(epoch.Unprotect().ok());
    // todo(optimization): replace tail call with loop? check if this is TCO by compiler? musttail?
    return insert(key, value);
  }

  md.visible = 1;
  auto *desc = desc_pool->AllocateDescriptor();
  assert(desc);
  desc->AddEntry((uint64_t*)&leaf->header.status_word, *(uint64_t*)&sw, *(uint64_t*)&sw);
  desc->AddEntry((uint64_t*)&nmd[sw.record_count-1], *(uint64_t*)&md_old, *(uint64_t*)&md);
  if (!desc->MwCAS()) {
    // node has unfortunately become frozen in the meantime
    // so we must retry the entire thing
    assert(epoch.Unprotect().ok());
    // todo(optimization): replace tail call with loop? check if this is TCO by compiler? musttail?
    return insert(key, value);
  }

  // all done, insert success
  assert(epoch.Unprotect().ok());
  return true;
}

bool BzTree::update(const std::string key, const std::string value) {
  if (DEBUG_PRINT_ACTIONS) printf("--- update %s %s\n", key.c_str(), value.c_str());
  // fail if the size required is larger than min free space, because the key can potentially cause rebalancing issues
  size_t space_required = key.length() + 1 + value.length() + 1;
  if (sizeof(struct NodeMetadata) + space_required > BZTREE_MIN_FREE_SPACE) return false;

  // now we start
  assert(epoch.Protect().ok());
  TOID(struct Node) leaf_oid = find_leaf(key, true);
  struct Node *leaf = D_RW(leaf_oid);
  struct NodeMetadata *nmd = reinterpret_cast<struct NodeMetadata*>(leaf->body);

  // todo(optimization): binary search sorted keys
  for (uint16_t i=0; i<leaf->header.status_word.record_count; i++) {
    if (nmd[i].visible && strcmp(&leaf->body[nmd[i].offset], key.c_str()) == 0) {
      // found! now we copy it into local and recheck
      // while (1) is to be able to retry upon new data region allocation failure, since the node is the same
      // (almost certainly, that is, it's rechecked for failures though so it's fine)
      while (1) {
        struct NodeHeaderStatusWord sw_old = leaf->header.status_word;
        struct NodeHeaderStatusWord sw = sw_old;
        struct NodeMetadata nmdi_old = nmd[i];
        struct NodeMetadata nmdi = nmdi_old;
        if (!nmdi.visible || sw.frozen) {
          // we have been bamboozled (potentially via a concurrent delete for the same node)
          // or the thing is frozen, either way, we must re-scan
          assert(epoch.Unprotect().ok());
          // todo(optimization): tail call
          return update(key, value);
        }

        // todo(feature): if the payload value is smaller, consider updating in-place
        // this is nontrivial though because there could be a concurrent read for the
        // value which cannot return a partial old and partial new value

        // todo(optimization): we really don't need to re-allocate the key here, but then
        // we would have to change the node data structure to have key and value ptrs
        // instead of a key len and value len and offset... which may be better, actually, sidenote:
        // we did take the design decision to only store strings, so nulls cannot be in k or v
        // so we can get away with one less pointer in the struct

        // now we need to reserve some space, or split the node if we can't
        size_t space_required = key.length() + 1 + value.length() + 1;
        if (sw.block_size + space_required > sizeof(struct Node) - sizeof(struct NodeHeader) -
            sw.record_count * sizeof(struct NodeMetadata)) {
          // too large to fit - something went wrong since we never should've seen this from find_leaf's SMOs
          assert(epoch.Unprotect().ok());
          assert(false);
          return false;
        }

        // allocate space first
        // todo(optimization): we only need a one-word CAS for this, but,
        // we're using the library for convenience
        sw.block_size += space_required;
        {
          auto *desc = desc_pool->AllocateDescriptor();
          assert(desc);
          desc->AddEntry((uint64_t*)&leaf->header.status_word, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
          if (!desc->MwCAS()) {
            // possible frozen or insert, optimistically continue, it'll detect frozen if so
            continue;
          }
        }

        // prepare next pmwcas
        sw_old = sw;
        // this one swaps in the offset and total_len, so we can also add in the delete_size
        // (not mentioned in paper, but it's a good heuristic thing to add)
        // since we need to pmwcas in the status word anyways to make sure the node didn't get frozen
        sw.delete_size += nmdi.total_len;

        nmdi.offset = sizeof(leaf->body) - sw.block_size;
        assert(nmdi.key_len == key.length() + 1);
        nmdi.total_len = key.length() + 1 + value.length() + 1;
        pmemobj_memcpy_persist(pop, &leaf->body[nmdi.offset], key.c_str(), key.length() + 1);
        pmemobj_memcpy_persist(pop, &leaf->body[nmdi.offset + nmdi.key_len], value.c_str(), value.length() + 1);

        // install new data offset
        {
          auto *desc = desc_pool->AllocateDescriptor();
          assert(desc);
          desc->AddEntry((uint64_t*)&leaf->header.status_word, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
          desc->AddEntry((uint64_t*)&nmd[i], *(uint64_t*)&nmdi_old, *(uint64_t*)&nmdi);
          if (!desc->MwCAS()) {
            // possible frozen or insert, optimistically continue, it'll detect frozen if so
            // todo(optimization): we could un-allocate the space... uhh, that's dangerous though
            continue;
          }
        }

        // all done!
        assert(epoch.Unprotect().ok());
        return true;
      }
    }
  }

  // we did not find the key
  assert(epoch.Unprotect().ok());
  return false;
}

std::optional<std::string> BzTree::lookup(const std::string key) {
  if (DEBUG_PRINT_ACTIONS) printf("--- lookup %s\n", key.c_str());
  // we still need protection against deletion for this
  assert(epoch.Protect().ok());

  TOID(struct Node) leaf_oid = find_leaf(key, false);
  const struct Node *leaf = D_RO(leaf_oid);
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(leaf->body);

  // todo(optimization): binary search sorted keys
  for (uint16_t i=0; i<leaf->header.status_word.record_count; i++) {
    if (nmd[i].visible && strcmp(&leaf->body[nmd[i].offset], key.c_str()) == 0) {
      // found!
      std::string res(&leaf->body[nmd[i].offset + nmd[i].key_len]);
      assert(epoch.Unprotect().ok());
      return res;
    }
  }

  // did not find it
  assert(epoch.Unprotect().ok());
  return std::nullopt;
}

bool BzTree::erase(const std::string key) {
  if (DEBUG_PRINT_ACTIONS) printf("--- erase %s\n", key.c_str());
  assert(epoch.Protect().ok());
  // todo(optimization): is perform_smo=true or false better here?
  TOID(struct Node) leaf_oid = find_leaf(key, false);
  struct Node *leaf = D_RW(leaf_oid);
  struct NodeMetadata *nmd = reinterpret_cast<struct NodeMetadata*>(leaf->body);

  // todo(optimization): binary search sorted keys
  for (uint16_t i=0; i<leaf->header.status_word.record_count; i++) {
    if (nmd[i].visible && strcmp(&leaf->body[nmd[i].offset], key.c_str()) == 0) {
      // found! now we copy it into local and recheck
      struct NodeHeaderStatusWord sw_old = leaf->header.status_word;
      struct NodeHeaderStatusWord sw = sw_old;
      struct NodeMetadata nmdi_old = nmd[i];
      struct NodeMetadata nmdi = nmdi_old;
      if (!nmdi.visible || sw.frozen) {
        // we have been bamboozled (potentially via a concurrent delete for the same node)
        // or the thing is frozen, either way, we must re-scan
        assert(epoch.Unprotect().ok());
        // todo(optimization): tail call
        return erase(key);
      }

      // erase node
      nmdi.offset = 0;
      nmdi.visible = 0;
      sw.delete_size += nmdi.total_len + sizeof(nmdi);

      // pmwcas to install new values
      auto *desc = desc_pool->AllocateDescriptor();
      assert(desc);
      desc->AddEntry((uint64_t*)&leaf->header.status_word, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
      desc->AddEntry((uint64_t*)&nmd[i], *(uint64_t*)&nmdi_old, *(uint64_t*)&nmdi);
      if (!desc->MwCAS()) {
        // node has unfortunately become frozen in the meantime, or something, we have to re-traverse
        assert(epoch.Unprotect().ok());
        // todo(optimization): tail call
        return erase(key);
      }

      return true;
    }
  }

  // we did not find the key
  assert(epoch.Unprotect().ok());
  return false;
}

void BzTree::destroy() {
  // todo(optimization): we should free all the nodes, otherwise they're still taking up memory
  // either that or just rm the pool object after closing it
  // todo(benchmarks): we may actually need to do the latter for benchmarks

  // destroy the tree root node
  D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj))->metadata = TOID_NULL(struct BzPMDKMetadata);
  D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj))->desc_pool = TOID_NULL(DescriptorPool);

  // clear decriptor pool and prevent reuse of this instance by resetting pop
  if (desc_pool) desc_pool->~DescriptorPool();
  desc_pool = nullptr;
  pop = nullptr;

  // clear aux stuff
  garbage.Uninitialize();
  epoch.Uninitialize();

  // clear tls
  Thread::ClearRegistry(true);
}

}  // namespace pmwcas
