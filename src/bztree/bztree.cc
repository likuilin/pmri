#include "bztree.h"
#include "common/allocator_internal.h"

namespace pmwcas {

BzTree::BzTree() {
#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  pop = allocator->GetPool();

  pmdkroot = POBJ_ROOT(pop, struct BzPMDKRootObj);

  TX_BEGIN(pop) {
    TX_ADD(pmdkroot);
    if (D_RO(D_RO(pmdkroot)->desc_pool) == nullptr || D_RO(D_RO(pmdkroot)->root_node) == nullptr) {
      // new bztree, initialize it
      D_RW(pmdkroot)->desc_pool = TX_NEW(DescriptorPool);
      D_RW(pmdkroot)->root_node = TX_NEW(struct Node);
      D_RW(pmdkroot)->height = 1;
    }
  } TX_END
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
}

bool BzTree::insert(const std::string key, uint64_t value) {
  // todo
  return true;
}

bool BzTree::update(const std::string key, uint64_t value) {
  // todo
  return true;
}

std::optional<uint64_t> BzTree::lookup(const std::string key) {
  // todo
  return 10;
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

uint64_t BzTree::iterator::operator*() const {
  // todo
  return 12;
}

}  // namespace pmwcas
