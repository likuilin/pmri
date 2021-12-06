#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {

TOID(struct Node) BzTree::node_compact(TOID(struct Node) node_oid) {
  // create new node
  const struct Node *node = D_RO(node_oid);
  TOID(struct Node) new_node_oid;
  POBJ_ZNEW(pop, &new_node_oid, struct Node);
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(&node->body);

  printf("Compacting\n");
  DEBUG_print_node(node);

  struct Node *new_node = D_RW(new_node_oid);
  struct NodeMetadata *new_nmd = reinterpret_cast<struct NodeMetadata*>(&new_node->body);

  // copy all nodes
  // todo(optimization): sort the keys here to allow for sorted optimizations
  uint16_t j = 0;
  uint32_t offset = sizeof(node->body);
  for (uint16_t i=0; i<node->header.status_word.record_count; i++) {
    if (!nmd[i].visible) continue;
    // copy metadata
    new_nmd[j].control = 0;
    new_nmd[j].visible = 1;
    offset -= nmd[i].total_len;
    new_nmd[j].offset = offset;
    new_nmd[j].key_len = nmd[i].key_len;
    new_nmd[j].total_len = nmd[i].total_len;
    new_node->header.status_word.record_count += 1;
    new_node->header.status_word.block_size += nmd[i].total_len;
    j++;

    // copy node heap
    pmemobj_memcpy_persist(pop, &new_node->body[offset], &node->body[nmd[i].offset], nmd[i].total_len);
  }

  printf("Compacting done\n");
  DEBUG_print_node(new_node);

          printf("????????????????a\n");
  return new_node_oid;
}

std::pair<TOID(struct Node), std::pair<TOID(struct Node), TOID(struct Node)>>
    BzTree::node_split(std::optional<TOID(struct Node)> parent, TOID(struct Node) node) {
  assert(0);
  // todo(req): impl
  return std::make_pair(node, std::make_pair(node, node));
}

TOID(struct Node) BzTree::node_merge(TOID(struct Node) parent, uint16_t idx,
    TOID(struct Node) ca, TOID(struct Node) chb) {
  assert(0);
  // todo(req): impl
  return parent;
}

}  // namespace pmwcas
