#include "bztree.h"
#include "common/allocator_internal.h"

#define POOL_SIZE 10000000
#define POOL_THREADS 10

namespace pmwcas {

BzTree::BzTree() {
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
    // todo: check if we have to initialize? DescriptorPool has a third param for existing vm addr...
    desc_pool = D_RW(D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj))->desc_pool);
  }
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
}

const struct BzPMDKMetadata *BzTree::get_metadata() {
  return D_RO(D_RO(POBJ_ROOT(pop, struct BzPMDKRootObj))->metadata);
}

std::optional<TOID(struct Node)> BzTree::find_leaf(const std::string key) {
}

bool BzTree::insert(const std::string key, const std::string value) {
  // todo
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
