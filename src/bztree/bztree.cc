#include "bztree.h"

namespace pmwcas {

void BzTree::insert(uint64_t key, uint64_t value) {
  // todo
}

std::optional<uint64_t> BzTree::lookup(uint64_t key) {
  // todo
  return 10;
}

void BzTree::erase(uint64_t key) {
  // todo
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
