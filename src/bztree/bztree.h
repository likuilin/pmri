#include <cstdint>
#include <iterator>
#include <optional>

#define BZTREE_NODE_CAPACITY 10

namespace pmwcas {

struct BzTree {
  public:
    // insert, lookup, erase
    void insert(uint64_t key, uint64_t value);
    std::optional<uint64_t> lookup(uint64_t key);
    void erase(uint64_t key);

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

    // key capacity, exposed for tests
    static const size_t kCapacity = BZTREE_NODE_CAPACITY;
};
}  // namespace pmwcas
