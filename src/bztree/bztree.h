#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include "mwcas/mwcas.h"
#include "common/garbage_list.h"

#pragma once  

// size in bytes of each node
// this should be = 16 + 16*(num keys) + total key len
// 16 for header, per key: 8 for metadata, 8 for value ptr, variable for key len
#define BZTREE_NODE_SIZE 256

// for testing: key len is 8, so 10 keys is 16+16*10+8*10 = 256 bytes exactly

namespace pmwcas {

// pmem stuff
// ref https://github.com/microsoft/pmwcas/blob/e9b2ba45257ff4e4b90c42808c068f3eda2f503d/src/environment/environment_linux.h#L439
// the example PMDKRootObj does it incorrectly, i'm pretty sure
// stores direct pointers in the pmem object, does restore even work?
// so, uh, let's just not use PMDKAllocator rather than fiddling with getting non-direct
// ptr back out of Allocate() call, and stuff like that, to save to pmem

// unfortunately, these cannot be scoped relative to where we want to use them
// ref https://pmem.io/2015/06/11/type-safety-macros.html
// TOID is done with macro magic that only supports two tokens in the TOID()
// to accommodate TOID(struct xyz), but neither ns::TOID(xyz) nor TOID(ns::xyz) will work as intended

// NodeHeader and NodeMetadata don't need to be in the layout
// because no persistent pointers to those types should ever be needed
// since they are calculated from offsets on the Node persistent pointer
POBJ_LAYOUT_BEGIN(bztree_layout);
POBJ_LAYOUT_ROOT(bztree_layout, struct BzPMDKRootObj);
POBJ_LAYOUT_ROOT(bztree_layout, struct BzPMDKMetadata);
POBJ_LAYOUT_TOID(bztree_layout, DescriptorPool);
POBJ_LAYOUT_TOID(bztree_layout, struct Node);
POBJ_LAYOUT_END(bztree_layout);

// ref figure 2 for these
#pragma pack(1)
struct NodeHeaderStatusWord {
  uint8_t control       : 3;
  bool frozen           : 1;
  // next three only exist for leaf nodes
  uint16_t record_count : 16;
  uint32_t block_size   : 22;
  uint32_t delete_size  : 22;
};
#pragma pack(1)
struct NodeHeader {
  uint32_t node_size    : 32;
  uint32_t sorted_count : 32;
  struct NodeHeaderStatusWord status_word;
};
static_assert(sizeof(struct NodeHeader) == 16);

#pragma pack(1)
struct NodeMetadata {
  uint8_t control       : 3;
  bool visible          : 1;
  uint32_t offset       : 28;
  uint16_t key_len      : 16; // note: includes trailing null
  uint16_t total_len    : 16; // note: includes trailing null for key and value
};
static_assert(sizeof(struct NodeMetadata) == 8);

#pragma pack(1)
struct Node {
  struct NodeHeader header;
  char body[BZTREE_NODE_SIZE - sizeof(header)];
};
static_assert(sizeof(struct Node) == BZTREE_NODE_SIZE);

// actual root object only contains a pointer to the root object and descriptor pool
// this is so we can atomically update height alongside a new root that's that high
// the paper does not address this problem
// this is a struct instead of a typedef because it must be accepted by TOID
struct BzPMDKRootObj {
  TOID(struct BzPMDKMetadata) metadata;
  TOID(DescriptorPool) desc_pool;
};

// root object contains root node and height and global index epoch
// multiple may exist if we're in the middle of a root rotation
struct BzPMDKMetadata {
  TOID(struct Node) root_node;
  uint64_t height;
  uint64_t global_epoch;
};

class BzTree {
  public:
    BzTree();
    ~BzTree();

    // insert, update, lookup, erase
    // these return false on failure, the user may retry if they want
    bool insert(const std::string key, const std::string value);
    bool update(const std::string key, const std::string value);
    std::optional<std::string> lookup(const std::string key);
    bool erase(const std::string key);

    // input iterator for range scan
    class iterator: public std::iterator<
                        std::input_iterator_tag,   // iterator_category
                        const std::string,         // value_type
                        size_t,                    // difference_type
                        const std::string*,        // pointer
                        const std::string          // reference
                                      >{
      public:
        iterator& operator++();
        iterator operator++(int);
        bool operator==(iterator other) const;
        bool operator!=(iterator other) const;
        const std::string operator*() const;
    };
    // these two mirror std::set's range iterators
    // lower_bound is an iterator for the first element >= key
    iterator lower_bound(const std::string key);
    // upper_bound is an iterator for the first element > key
    iterator upper_bound(const std::string key);

    // used to destroy the tree, so that a new tree can be constructed
    // the destructor doesn't actually destroy the tree, because it is saved in pmem
    // not thread safe, will cause UB if called while other operations are ongoing
    void destroy();

    // prints stuff to stdout, without regard for safety
    void DEBUG_print_node(const struct Node* node);
    void DEBUG_print_tree(TOID(struct Node) node_oid = TOID_NULL(struct Node), int h = 0, int height = 0);

  private:
    // we must re-obtain the root pointer on every action, so nothing in pmem can really be "cached"
    PMEMobjpool *pop;

    // garbage collection
    EpochManager epoch;
    GarbageList garbage;

    // these are cached from the pmem safely because they never are changed
    DescriptorPool *desc_pool;
    uint64_t global_epoch;

    // destroy function for garbage list
    static void DestroyNode(void *destroyContext, void *p) {
#ifdef PMDK
      POBJ_FREE(p);
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
    };

    // === helpers ===

    // get metadata struct from pop
    const struct BzPMDKMetadata *get_metadata();

    // traverses the tree and finds the leaf node in which the key should be
    // expects the gc to be already protected
    TOID(struct Node) find_leaf(const std::string key);

    // like find_leaf but it returns tuple(the leaf, the parent, id in parent) instead,
    // all the info needed for structural modifications
    // since inner nodes are immutable, this is safe
    // if the leaf is the root node then parent is nullopt and id is zero
    // since when the leaf is root, the parent must be the same for future algorithms,
    // we must use the caller's md, because if the height changes from or to 1 between calls,
    // there will be inconsistency
    // expects the gc to be already protected
    std::tuple<TOID(struct Node), std::optional<TOID(struct Node)>, uint16_t>
      find_leaf_parent(const std::string key, const struct BzPMDKMetadata *md);

    // === structural modifications (SMOs) ===
    // note: all of these invalidate the tree if they return true
    // so, you must unprotect before calling them, and the only safe thing to do after calling them
    // is re-traverse the tree down from a new get_metadata root
    // returning false means no changes can be made, failures are retried

    // makes the amount of space in the node, first by trying to compact the node, then by splitting
    // void node_make_space(const std::string key, size_t space_required);

    // compacts node, making deleted key space available and (todo) sorting the keys
    bool node_compact(const std::string key);

    // splits node once
    // void node_split(const std::string key)
};
}  // namespace pmwcas
