#include "bztree.h"
#include "common/allocator_internal.h"

#define POOL_SIZE 10000000
#define POOL_THREADS 10

namespace pmwcas {

BzTree::BzTree() {
  assert(epoch.Initialize().ok());
  assert(garbage.Initialize(&epoch).ok());

#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  pop = allocator->GetPool();

  const struct BzPMDKMetadata *metadata = this->get_metadata();

  if (metadata == nullptr) {
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
    // grab the descriptor pool ptr
    // todo(persistence): check if we have to initialize? DescriptorPool has a third param for existing vm addr...
    desc_pool = D_RW(D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj))->desc_pool);
    // todo(persistence): after verifying desc_pool works: increment the global epoch
    // also throw if global epoch cannot fit in 28 bits, because we borrow offset for that
    global_epoch = 1;
  }
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
}

BzTree::~BzTree() {
  if (desc_pool) desc_pool->~DescriptorPool();
  garbage.Uninitialize();
  epoch.Uninitialize();
  Thread::ClearRegistry(true);
}

const struct BzPMDKMetadata *BzTree::get_metadata() {
  return D_RO(D_RO(POBJ_ROOT(pop, struct BzPMDKRootObj))->metadata);
}

TOID(struct Node) BzTree::find_leaf(const std::string key) {
  auto *md = get_metadata();

  for (size_t i=0; i<md->height-1; i++) {
    // todo(req): dereference inner nodes
    assert(0);
  }

  return md->root_node;
}

/*
std::optional<uint64_t> leaf_find_key(TOID(struct Node) node, const std::string key, bool only_visible) {
  const struct NodeHeader *header = reinterpret_cast<const struct NodeHeader*>(D_RO(node));
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(header + 1);
  for (uint16_t i=0; i<header->record_count; i++) {
    if (only_visible && !nmd[i].visible) continue;
    
  }
}
*/

bool BzTree::insert(const std::string key, const std::string value) {
  size_t space_required = sizeof(struct NodeMetadata) + key.length() + 1 + value.length() + 1;
  // exit early if it is too large for any node
  if (space_required > sizeof(struct Node) - sizeof(struct NodeHeader)) return false;

  assert(epoch.Protect());
  TOID(struct Node) leaf_oid = find_leaf(key);
  const struct Node *leaf = D_RO(leaf_oid);
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(leaf->body);

  // check for existing value
  // this first pass is only opportunistic, it's not to formally check for existing value
  // it catches the common bad insertion case though
  bool recheck = false;
  // todo(optimization): binary search sorted keys
  for (uint16_t i=0; i<leaf->header.status_word.record_count; i++) {
    // any not-visible ones potentially are key conflicts in the middle of insertion, if in the same epoch
    if (!nmd[i].visible && nmd[i].offset == global_epoch) recheck = true;
    if (nmd[i].visible && strcmp(&leaf->body[nmd[i].offset], key.c_str()) == 0) {
      // fail because we found one that's already the same key
      assert(epoch.Unprotect());
      return false;
    }
  }

  // reserve space for metadata and key value entry
  while (1) {
    struct NodeHeaderStatusWord sw_old = leaf->header.status_word;
    struct NodeHeaderStatusWord sw = sw_old;
    if (sw.block_size + space_required > sizeof(struct Node) - sizeof(struct NodeHeader) -
        sw.record_count * sizeof(struct NodeMetadata)) {
      // too large to fit, we have to split the node
      // todo(req): split node
      assert(false);
    }
    sw.block_size += key.length() + 1 + value.length() + 1;
    sw.record_count += 1;

    // pmwcas to allocate space on the node
    auto *desc = desc_pool->AllocateDescriptor();
    assert(desc);
    desc->AddEntry((uint64_t*)&leaf->header.status_word, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
    // desc->AddEntry((uint64_t*)&nmd[sw_old.record_count].offset, 0, // TODO PROGRESS BOOKMARK
    
  }

  // todo
  assert(epoch.Unprotect());
  return true;
}

bool BzTree::update(const std::string key, const std::string value) {
  // todo
  return true;
}

std::optional<std::string> BzTree::lookup(const std::string key) {
  // todo
  return "todo";
}

bool BzTree::erase(const std::string key) {
  // todo
  return true;
}

// post increment
BzTree::iterator& BzTree::iterator::operator++() {
  // todo
  return *this;
}

// pre increment
BzTree::iterator BzTree::iterator::operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

bool BzTree::iterator::operator==(BzTree::iterator other) const {
  // todo
  return true;
}

bool BzTree::iterator::operator!=(BzTree::iterator other) const {
  return !(*this == other);
}

const std::string BzTree::iterator::operator*() const {
  // todo
  return "todo";
}

}  // namespace pmwcas
