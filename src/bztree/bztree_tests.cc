#include <gtest/gtest.h>
#include "common/allocator_internal.h"
#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {
namespace test {

// most test patterns shamelessly borrowed from lab 3's BTree tests

GTEST_TEST(BzTreeTest, LookupEmptyTree) {
  BzTree tree;

  auto test = "searching for a non-existing element in an empty B-Tree";
  ASSERT_FALSE(tree.lookup(42)) << test << " seems to return something :-O";

  Thread::ClearRegistry();
}

GTEST_TEST(BzTreeTest, LookupSingleLeaf) {
  BzTree tree;

  // Fill one page
  for (auto i = 0ul; i < BzTree::kCapacity; ++i) {
    tree.insert(i, 2 * i);
    ASSERT_TRUE(tree.lookup(i))
        << "searching for the just inserted key k=" << i << " yields nothing";
  }

  // Lookup all values
  for (auto i = 0ul; i < BzTree::kCapacity; ++i) {
    auto v = tree.lookup(i);
    ASSERT_TRUE(v) << "key=" << i << " is missing";
    ASSERT_EQ(*v, 2 * i) << "key=" << i << " should have the value v=" << 2 * i;
  }

  Thread::ClearRegistry();
}

GTEST_TEST(BzTreeTest, LookupSingleSplit) {
  BzTree tree;

  // Insert values
  for (auto i = 0ul; i < BzTree::kCapacity; ++i) {
    tree.insert(i, 2 * i);
  }

  tree.insert(BzTree::kCapacity, 2 * BzTree::kCapacity);
  ASSERT_TRUE(tree.lookup(BzTree::kCapacity))
      << "searching for the just inserted key k="
      << (BzTree::kCapacity + 1) << " yields nothing";

  // Lookup all values
  for (auto i = 0ul; i < BzTree::kCapacity + 1; ++i) {
    auto v = tree.lookup(i);
    ASSERT_TRUE(v) << "key=" << i << " is missing";
    ASSERT_EQ(*v, 2 * i) << "key=" << i << " should have the value v=" << 2 * i;
  }

  Thread::ClearRegistry();
}

} // namespace test
} // namespace pmwcas

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_minloglevel = 2;
#ifdef WIN32
  pmwcas::InitLibrary(pmwcas::DefaultAllocator::Create,
                           pmwcas::DefaultAllocator::Destroy,
                           pmwcas::WindowsEnvironment::Create,
                           pmwcas::WindowsEnvironment::Destroy);
#else
#ifdef PMDK
  pmwcas::InitLibrary(pmwcas::PMDKAllocator::Create("doubly_linked_test_pool",
                                                    "doubly_linked_layout",
                                                    static_cast<uint64_t >(1024) * 1024 * 1204 * 5),
                      pmwcas::PMDKAllocator::Destroy,
                      pmwcas::LinuxEnvironment::Create,
                      pmwcas::LinuxEnvironment::Destroy);
#else
  pmwcas::InitLibrary(pmwcas::TlsAllocator::Create,
                           pmwcas::TlsAllocator::Destroy,
                           pmwcas::LinuxEnvironment::Create,
                           pmwcas::LinuxEnvironment::Destroy);
#endif  // PMDK
#endif  // WIN32

  return RUN_ALL_TESTS();
}
