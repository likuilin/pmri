#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {

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
