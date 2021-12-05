#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {

// todo(optimization): sort the keys during these ops

TOID(struct Node) BzTree::node_compact(TOID(struct Node) node) {
  // todo(req): impl
  return node;
}

std::pair<TOID(struct Node), std::pair<TOID(struct Node), TOID(struct Node)>>
    BzTree::node_split(std::optional<TOID(struct Node)> parent, TOID(struct Node) node) {
  // todo(req): impl
  return std::make_pair(node, std::make_pair(node, node));
}

TOID(struct Node) BzTree::node_merge(TOID(struct Node) parent, uint16_t idx,
    TOID(struct Node) ca, TOID(struct Node) chb) {
  // todo(req): impl
  return parent;
}

}  // namespace pmwcas
