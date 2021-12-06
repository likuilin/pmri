#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include "mwcas/mwcas.h"
#include "common/garbage_list.h"

#pragma once  

// size in bytes of each node
// this should be = 16 + 16*(num keys) + total key len
// 16 for header, per key: 8 for metadata, 8 for value ptr, variable for key 
#define BZTREE_NODE_SIZE 256

// minimum free space for node to not be split during non-read traversal
// warning: keys larger than this minus 16 (nmd + child ptr) currently cannot be inserted
// todo(feature): add this - it'd require a lot of design decision though, since we shouldn't assume
// that the large key needs to be propagated upwards every time we traverse the tree for an insertion, right
// but, on the other hand, we don't want to have to keep track of /all/ the ancestors during traversal
// since that introduces a variably sized data structure depending on the height
#define BZTREE_MIN_FREE_SPACE 40

// max free space before a node is merged
#define BZTREE_MAX_FREE_SPACE 128

// maximum deleted space before a node is compacted
#define BZTREE_MAX_DELETED_SPACE 100

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
  uint16_t record_count : 16;
  uint32_t block_size   : 22;
  uint32_t delete_size  : 22;
};
#pragma pack(1)
struct NodeHeader {
  uint32_t node_size    : 32; // note: unused, our nodes are constant size
  uint32_t sorted_count : 32; // note: unused, we do not have sorted keys yet
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
      auto oid_ptr = pmemobj_oid(p);
      TOID(char) ptr_cpy;
      TOID_ASSIGN(ptr_cpy, oid_ptr);
      POBJ_FREE(&ptr_cpy);
#else
#error "Non-PMDK not implemented"
#endif  // PMDK
    };

    // === helpers ===

    // get metadata struct from pop
    struct BzPMDKMetadata *get_metadata();

    // traverses the tree to find where a key would go, if not in the tree already
    // if perform_smo is on, will heuristically SMO nodes that need it and re-launch itself
    // required except if we're traversing to read, because otherwise, there may not be room to insert
    // either at the leaf or somewhere along the ancestor chain, not necessarily
    // expects the gc to be already protected
    TOID(struct Node) find_leaf(const std::string key, bool perform_smo);

    // like (and used by) find_leaf but it returns tuple(the leaf, the parent, id in parent) instead,
    // all the info needed for structural modifications, in order to be recursively called
    // if parent is nullopt then the node is the root
    // expects the gc to be already protected
    std::tuple<TOID(struct Node), std::optional<TOID(struct Node)>, uint16_t>
        find_leaf_parent(const std::string key, bool perform_smo);

    // implementation for find_leaf_parent and find_leaf, so that it can potentially fail
    // and also perform any SMOs needed during traversal
    // if repeatedly called on the same tree, will only fail up to O(height of tree) times
    // if it fails, then we need to acquire a new md, since root could have changed
    // expects the gc to be already protected
    std::optional<std::tuple<TOID(struct Node), std::optional<TOID(struct Node)>, uint16_t>>
        find_leaf_parent_smo(const std::string key, bool perform_smo, struct BzPMDKMetadata *md);

    // helpers for getting and setting the pmdk offset of a TOID
    // note: this is breaking into pmdk internals, but necessary because
    // in the node we want to only store offset pointers
    static inline void toid_set_offset(TOID(struct Node) target, uint64_t off);
    static inline uint64_t toid_get_offset(TOID(struct Node) target);

    // helper for adding a TOID to a mwcas descriptor
    // this is required in a few places because TOIDs are not actually one word, so they
    // can't be atomically set in-place
    // TOID(T) is a macro that ends up using ##T, so we can't template it like TOID(T) sadly
    template<typename T> static void desc_add_toid(Descriptor *desc, T *loc, T a, T b);

    // helper for swapping out a node pointer inside a node or inside the root
    // this is the only safe thing to do without freezing a node
    // returns if it fails (only if the node freezes)
    bool swap_node(std::optional<TOID(struct Node)> parent, uint64_t *node_off_ptr,
        TOID(struct Node) old_node, TOID(struct Node) new_node);

    // === structural modifications (SMOs) ===
    // note: all of these invalidate the tree if they return true
    // so, you must unprotect before calling them, and the only safe thing to do after calling them
    // is re-traverse the tree down from a new get_metadata root
    // returning false means no changes can be made, not necessarily that it failed -
    // retryable failures are retried
    // all of these expect the gc to be already protected

    // compacts node, making deleted key space available and (todo) sorting the keys
    // returns allocated new node, does not delete old node
    // new node must be spliced into parent
    TOID(struct Node) node_compact(TOID(struct Node) node);

    // splits node once
    // returns allocated new parent and the two children (for deleting on failure), does not delete old nodes
    // new parent must be spliced into grandparent of the split nodes
    // if parent is nullopt, then a new parent is created (split of root)
    std::pair<TOID(struct Node), std::pair<TOID(struct Node), TOID(struct Node)>>
        node_split(std::optional<TOID(struct Node)> parent, TOID(struct Node) node);

    // merges sibling nodes
    // takes the parent node and the index of the first of the two children, and the two children
    // this is so that our pmwcas can ensure that the children are what they used to be
    // returns allocated new parent, does not delete old nodes
    // new parent must be spliced into grandparent of the merged nodes
    TOID(struct Node) node_merge(TOID(struct Node) parent, uint16_t idx,
        TOID(struct Node) ca, TOID(struct Node) chb);
};
}  // namespace pmwcas
