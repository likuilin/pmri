#include "bztree.h"

namespace pmwcas {

BzTree::BzTree(DescriptorPool *desc_pool_, void *buf_, size_t max_nodes_)
    : desc_pool(desc_pool_), max_nodes(max_nodes_) {
  header = reinterpret_cast<NodeHeader *>(buf_);
  metadata = reinterpret_cast<NodeMetadata *>(header + 1);
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
