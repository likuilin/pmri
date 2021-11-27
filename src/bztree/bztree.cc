#include "bztree.h"
#include "common/allocator_internal.h"

namespace pmwcas {

BzTree::BzTree() {
#ifdef PMDK
  auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
  pop = allocator->GetPool();

  TOID(struct BzRootObj) root = POBJ_ROOT(pop, struct BzRootObj);

  TX_BEGIN(pop) {
    TX_ADD(root);
    desc_pool = D_RO(root)->desc_pool;
    root_node = D_RO(root)->root_node;

    if (D_RO(desc_pool) == nullptr || D_RO(root_node) == nullptr) {
      // new bztree, initialize it
      TX_ADD(root);
      desc_pool = D_RW(root)->desc_pool = TX_NEW(DescriptorPool);
      root_node = D_RW(root)->root_node = TX_NEW(struct Node);
    }
  }
  TX_ONABORT { printf("bztree init fail"); } TX_END
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
}

bool BzTree::insert(const std::vector<uint8_t> key, uint64_t value) {
  // todo
  return true;
}

bool BzTree::update(const std::vector<uint8_t> key, uint64_t value) {
  // todo
  return true;
}

std::optional<uint64_t> BzTree::lookup(const std::vector<uint8_t> key) {
  // todo
  return 10;
}

bool BzTree::erase(const std::vector<uint8_t> key) {
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
