#include "bztree.h"

namespace pmwcas {

void BzTree::DEBUG_print_node(const struct Node* node) {
  printf("=== node %p / %lx ===\n", node, pmemobj_oid(node).off);
  if (!node) return;
  printf("node_size:    %d\n", node->header.node_size);
  printf("sorted_count: %d\n", node->header.sorted_count);
  printf("-\n");
  printf("control:      %d\n", node->header.status_word.control);
  printf("frozen:       %d\n", node->header.status_word.frozen);
  printf("record_count: %d\n", node->header.status_word.record_count);
  printf("block_size:   %d\n", node->header.status_word.block_size);
  printf("delete_size:  %d\n", node->header.status_word.delete_size);
  printf("-\n");
  printf("data block:\n");
  assert(sizeof(node->body) % 16 == 0);
  for (size_t i=0; i<sizeof(node->body); i+=16) {
    for (size_t j=0; j<16; j++) printf("%02x ", (unsigned char)node->body[i+j]);
    printf("| ");
    for (size_t j=0; j<16; j++) {
      if (node->body[i+j] >= 0x20 && node->body[i+j] < 0x7f) printf("%c", node->body[i+j]);
      else printf(".");
    }
    printf("\n");
  }
}

void BzTree::DEBUG_print_tree(TOID(struct Node) node_oid /*= TOID_NULL(struct Node)*/, int h /*= 0*/, int height /*= 0*/) {
  if (TOID_IS_NULL(node_oid)) {
    auto *md = get_metadata();

    printf("=== TREE ===\n");
    printf("height:       %lu\n", md->height);
    printf("global epoch: %lu\n", md->global_epoch);
    printf("\n");
    printf("root "); // hack to prefix root with one space before first node, lol
    DEBUG_print_tree(md->root_node, 1, md->height);
    return;
  }

  const struct Node *node = D_RO(node_oid);

  printf("%*s%s node %p / %lx {\n", h*2-2, "", h == height ? "leaf" : "inner", node, pmemobj_oid(node).off);

  const struct NodeHeader *header = &node->header;
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(header + 1);

  for (size_t i=0; i<header->status_word.record_count; i++) {
    if (h != height) {
      // inner node
      printf("%*skey=%s\n", h*2, "", &node->body[nmd[i].offset]);

      // warning: here we are reaching into TOID internals to get and set offset
      TOID(struct Node) child;
      child.oid.pool_uuid_lo = node_oid.oid.pool_uuid_lo;
      child.oid.off = *(uint64_t*)&node->body[nmd[i].offset + nmd[i].key_len];
      if (TOID_IS_NULL(child)) printf("%*s(null)\n", h*2, "");
      else DEBUG_print_tree(child, h+1, height);
      // extra newline to separate out key value sections
    } else {
      // child node
      printf("%*skey=%s value=%s\n", h*2, "",
        &node->body[nmd[i].offset], &node->body[nmd[i].offset + nmd[i].key_len]);
    }
  }
  printf("%*s}\n", h*2-2, "");
}

void BzTree::DEBUG_verify_sorted(TOID(struct Node) node_oid) {
  const struct Node *node = D_RO(node_oid);
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(&node->body);

  const char *last = &node->body[nmd[0].offset];
  printf("=-= DEBUG_verify_sorted\n0. %s\n", last);
  bool sorted = true;
  for (size_t i=1; i<node->header.status_word.record_count; i++) {
    if (!nmd[i].visible) continue;
    const char *curr = &node->body[nmd[i].offset];
    printf("%ld. %s\n", i, curr);
    if (strcmp(last, curr) >= 0) sorted = false;
    last = curr;
  }
  if (!sorted) {
    DEBUG_print_tree();
    DEBUG_print_node(D_RO(node_oid));
  }
  assert(sorted);
}

}  // namespace pmwcas
