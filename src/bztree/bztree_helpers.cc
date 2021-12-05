#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {

const struct BzPMDKMetadata *BzTree::get_metadata() {
  return D_RO(D_RO(POBJ_ROOT(pop, struct BzPMDKRootObj))->metadata);
}

TOID(struct Node) BzTree::find_leaf(const std::string key) {
  auto *md = get_metadata();
  if (md->height == 1) return md->root_node;

  auto [leaf, parent, idx] = find_leaf_parent(key, md);
  return leaf;
}

std::tuple<TOID(struct Node), TOID(struct Node), uint16_t>
    BzTree::find_leaf_parent(const std::string key, const struct BzPMDKMetadata *md) {
  assert(md->height > 1);

  TOID(struct Node) child;
  TOID(struct Node) parent = md->root_node;

  uint16_t i;
  // perform height-1 dereferences to get to leaf
  uint64_t h=0;
  while (true) { // for (uint64_t h=0; h<md->height-1; h++) {
    // the inner nodes do not have a full TOID!
    // the spec dictates that they are 8 bytes, which means we don't have enough space for pool id
    // the tradeoff is that this means inner nodes can hold more keys, but, bztrees cannot span pools
    // for us, we need to reconstitute the TOID from the parent's pool id and the offset in the node body
    const struct NodeHeader *header = &D_RO(parent)->header;
    const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(header + 1);

    // todo(optimization): binary search sorted keys, inner nodes are guaranteed to
    // be sorted because they are immutable
    for (i=0; i<header->status_word.record_count; i++) {
      assert(nmd[i].total_len == nmd[i].key_len + 8); // optimization to use 8 for value len
      if (strcmp(&D_RO(parent)->body[nmd[i].offset], key.c_str()) >= 0) break;
    }
    uint64_t offset = *(uint64_t*)&D_RO(parent)->body[nmd[i].offset + nmd[i].key_len];

    // warning: here we are reaching into TOID internals to get and set offset
    child.oid.pool_uuid_lo = parent.oid.pool_uuid_lo;
    child.oid.off = offset;

    // if we're at the end, then return
    if (++h >= md->height-1) return std::make_tuple(child, parent, i);

    // prepare next iteration
    parent = child;
  }
}

}  // namespace pmwcas
