#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {

bool BzTree::node_compact(const std::string key) {
  auto *md = get_metadata();
  if (md->height == 1) return false;

  auto [leaf, parent, idx] = find_leaf_parent(key, md);
  return true;
}

  
  // todo(optimization): sort the keys here

}  // namespace pmwcas
