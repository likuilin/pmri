#include "bztree.h"
#include "include/pmwcas.h"

#define DEBUG_PRINT_SMOS 1

namespace pmwcas {

// since SMOs are all infrequent and slow (hopefully), we implement these simply with vectors in main memory
// note: these strings contain the end null character explicitly in the std::string! this is because we cannot
// treat values as c strings, because even though our interface has them as c strings, for inner nodes, values
// are pointers and may have nulls and will therefore be resized incorrectly

// copy all the key value pairs into a vector of pairs and sort them
std::vector<std::pair<std::string, std::string>> BzTree::copy_out(TOID(struct Node) node_oid) {
  const struct Node *node = D_RO(node_oid);
  const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(&node->body);

  std::vector<std::pair<std::string, std::string>> ret;
  for (uint16_t i=0; i<node->header.status_word.record_count; i++) {
    if (!nmd[i].visible) continue;
    std::string key(&node->body[nmd[i].offset], nmd[i].key_len);
    std::string val(&node->body[nmd[i].offset + nmd[i].key_len], nmd[i].total_len - nmd[i].key_len);
    ret.push_back(std::make_pair(key, val));
  }
  // todo(optimization): only sort ones outside sorted_count?
  std::sort(ret.begin(), ret.end(), [](auto &a, auto &b) {
    return a.first < b.first;
  });
  return ret;
}

// copy all the key value pairs back into a new node
// expects pairs to be sorted
TOID(struct Node) BzTree::copy_in(std::vector<std::pair<std::string, std::string>> pairs) {
  // create new node
  TOID(struct Node) node_oid;
  POBJ_ZNEW(pop, &node_oid, struct Node);

  struct Node *node = D_RW(node_oid);
  struct NodeMetadata *new_nmd = reinterpret_cast<struct NodeMetadata*>(&node->body);

  // add each key value pair in order to the new node
  uint16_t record_count = 0;
  uint32_t offset = sizeof(node->body);
  for (auto [key, val] : pairs) {
    new_nmd[record_count].control = 0;
    new_nmd[record_count].visible = 1;
    offset -= key.length() + val.length();
    new_nmd[record_count].offset = offset;
    new_nmd[record_count].key_len = key.length();
    new_nmd[record_count].total_len = key.length() + val.length();
    record_count++;

    // note: key and val include the terminating null in the string itself (so c_str has two terminating nulls)
    pmemobj_memcpy_persist(pop, &node->body[offset], key.c_str(), key.length());
    pmemobj_memcpy_persist(pop, &node->body[offset + key.length()], val.c_str(), val.length());
  }
  node->header.status_word.record_count = record_count;
  node->header.status_word.block_size = sizeof(node->body) - offset;
  node->header.sorted_count = record_count;

  return node_oid;
}

TOID(struct Node) BzTree::node_compact(TOID(struct Node) node_oid) {
  if (DEBUG_PRINT_SMOS) printf("--- compact\n");
  // in and out, real quick, 20 minute adventure
  return copy_in(copy_out(node_oid));
}

std::pair<TOID(struct Node), std::pair<TOID(struct Node), TOID(struct Node)>>
    BzTree::node_split(std::optional<TOID(struct Node)> parent, TOID(struct Node) node) {
  if (DEBUG_PRINT_SMOS) printf("--- split\n");
  // this might be a child, so we don't know that the keys are sorted
  auto sorted = copy_out(node);

  // need at least 3 nodes to split
  assert(sorted.size() > 2);

  // find the pivot point, halfway through the key sizes - sep will be the the elevated element
  uint64_t total_key_len = 0;
  for (auto [key, val] : sorted) total_key_len += key.size();
  auto sep = sorted.begin();
  uint64_t seen_key_len = 0;
  while (sep != sorted.end()) {
    seen_key_len += sep++->first.size();
    if (seen_key_len + seen_key_len > total_key_len) break;
  }
  // should not be possible since at the last element we must have broke, 2x > x
  assert(sep != sorted.end());
  // slightly more possible, so guard it just in case
  if (sep + 1 == sorted.end()) sep--;

  // now things [0, sep) are left, (sep, end) are right
  std::vector<std::pair<std::string, std::string>> left(sorted.begin(), sep);
  std::vector<std::pair<std::string, std::string>> right(sep, sorted.end());

  // create new nodes
  TOID(struct Node) new_left_oid = copy_in(left);
  TOID(struct Node) new_right_oid = copy_in(right);

  // right needs a dummy first key for scanning
  right.insert(right.begin(), std::make_pair(left.back().first, ""));

  // convert pool offsets of new nodes as strings
  std::string left_ptr((char*)&new_left_oid.oid.off, 8);
  std::string right_ptr((char*)&new_right_oid.oid.off, 8);

  // convert parent
  std::vector<std::pair<std::string, std::string>> parent_kv;
  if (parent.has_value()) {
    // existing parent, so copy most of the keys
    parent_kv = copy_out(*parent);

    // find the index of the node in the parent
    // todo(optimization): this is horrible, please fix this by altering the interface to node_split
    auto i = parent_kv.begin();
    while (++i != parent_kv.end()) {
      // make this less questionable, first three bits are used by pmwcas, though, ick
      if (((*(uint64_t*)i->second.c_str()) & ~0x7) == (node.oid.off & ~0x7)) break;
    }
    assert(i != parent_kv.end());

    // an item before i should be inserted with the last key of first
    parent_kv.insert(i, std::make_pair(left.back().first, left_ptr));
    // then after that, the item at i should have its value replaced with ptr to right
    *i = std::make_pair(i->first, right_ptr);
  } else {
    // new parent, probably new root - in this case we need the first one to have a key of ""
    parent_kv.push_back(std::make_pair("", ""));
    parent_kv.push_back(std::make_pair(left.back().first, left_ptr));
    parent_kv.push_back(std::make_pair(right.back().first, right_ptr));
  }
  TOID(struct Node) new_parent_oid = copy_in(parent_kv);

  return std::make_pair(new_parent_oid, std::make_pair(new_left_oid, new_right_oid));
}

TOID(struct Node) BzTree::node_merge(TOID(struct Node) parent, uint16_t idx,
    TOID(struct Node) ca, TOID(struct Node) chb) {
  if (DEBUG_PRINT_SMOS) printf("--- merge\n");
  assert(0);
  // todo(req): impl
  return parent;
}

}  // namespace pmwcas
