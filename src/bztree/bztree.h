#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>
#include "mwcas/mwcas.h"

// size in bytes of each node
// this should be = 16 + 16*(num keys) + total key len
// 16 for header, per key: 8 for metadata, 8 for value ptr, variable for key len
#define BZTREE_NODE_SIZE 256

// for testing: key len is 8, so 10 keys is 16+16*10+8*10 = 256 bytes exactly

namespace pmwcas {

class BzTree {
  public:
    // desc_pool is from pmwcas, buf is a ptr to a buffer of size BZTREE_NODE_SIZE*max_nodes in pmem
    BzTree(DescriptorPool *desc_pool_, void *buf_, size_t max_nodes_);

    // insert, update, lookup, erase
    bool insert(const std::vector<uint8_t> key, uint64_t value);
    bool update(const std::vector<uint8_t> key, uint64_t value);
    std::optional<uint64_t> lookup(const std::vector<uint8_t> key);
    bool erase(const std::vector<uint8_t> key);

    // input iterator for range scan
    class iterator: public std::iterator<
                        std::input_iterator_tag,   // iterator_category
                        uint64_t,                  // value_type
                        size_t,                    // difference_type
                        uint64_t*,                 // pointer
                        uint64_t                   // reference
                                      >{
      public:
        iterator& operator++();
        iterator operator++(int);
        bool operator==(iterator other) const;
        bool operator!=(iterator other) const;
        uint64_t operator*() const;
    };
    // these two mirror std::set's range iterators
    // lower_bound is an iterator for the first element >= key
    iterator lower_bound(uint64_t key);
    // upper_bound is an iterator for the first element > key
    iterator upper_bound(uint64_t key);

  private:
    // ref figure 2 for these
    #pragma pack(1)
    struct NodeHeader {
      uint32_t node_size    : 32;
      uint8_t control       : 3;
      bool frozen           : 1;
      uint16_t record_count : 16;
      uint32_t block_size   : 22;
      uint32_t delete_size  : 22;
      uint32_t sorted_count : 32;
    };
    static_assert(sizeof(struct NodeHeader) == 16);
    struct NodeMetadata {
      uint8_t control       : 3;
      bool visible          : 1;
      uint32_t offset       : 28;
      uint16_t key_len      : 16;
      uint16_t total_len    : 16;
    };
    static_assert(sizeof(struct NodeMetadata) == 8);

    size_t max_nodes;
    DescriptorPool *desc_pool;
    NodeHeader *header;
    NodeMetadata *metadata;
};
}  // namespace pmwcas
