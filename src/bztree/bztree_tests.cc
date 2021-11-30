#include <gtest/gtest.h>
#include "common/allocator_internal.h"
#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {
namespace test {

// most test patterns shamelessly borrowed from lab 3's BTree tests

// for testing: key len is 8 and val len is 8, so 10 keys is 16+16*10+8*10 = 256 bytes exactly
#define BZTREE_CAPACITY 10

struct SingleThreadTest {
  BzTree tree;

  SingleThreadTest() {
    MwCASMetrics::ThreadInitialize();
  }

  ~SingleThreadTest() {
    Thread::ClearRegistry();
  }
};

GTEST_TEST(BzTreeTest, LookupEmptyTree) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  auto test = "searching for a non-existing element in an empty B-Tree";
  ASSERT_FALSE(t->tree.lookup("abcd")) << test << " seems to return something :-O";
}

GTEST_TEST(BzTreeTest, LookupSinglePair) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  t->tree.insert("key", "value");

  ASSERT_TRUE(t->tree.lookup("key")) << "searching for an existing element in a one-value B-Tree";
  ASSERT_FALSE(t->tree.lookup("asdfasd")) << "searching for a non-existing element in a one-value B-Tree";
}

// todo(req) variable key value length tests

std::string _kid(uint64_t i) {
    // returns 7 chars, 8 with null
    char buf[8];
    snprintf(buf, 8, "k%06lu", i);
    return std::string(buf);
}
std::string _vid(uint64_t i) {
    // returns 7 chars, 8 with null
    char buf[8];
    snprintf(buf, 8, "v%06lu", i);
    return std::string(buf);
}

GTEST_TEST(BzTreeTest, LookupSingleLeaf) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  // Fill one page
  for (auto i = 0; i < BZTREE_CAPACITY; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    t->tree.insert(kid, vid);
    ASSERT_TRUE(t->tree.lookup(kid))
        << "searching for the just inserted key k=" << kid << " yields nothing";
  }

  // Lookup all values
  for (auto i = 0; i < BZTREE_CAPACITY; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    auto v = t->tree.lookup(kid);
    ASSERT_TRUE(v) << "key=" << kid << " is missing";
    ASSERT_TRUE(v == vid) << "key=" << kid << " wrong value";
  }

  ASSERT_FALSE(t->tree.insert("key", "value")) << "insertion causing node split";
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
  pmwcas::InitLibrary(pmwcas::PMDKAllocator::Create("bztree_test_pool",
                                                    "bztree_layout",
                                                    static_cast<uint64_t>(1024) * 1024 * 1024),
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
