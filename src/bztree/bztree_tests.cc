#include <gtest/gtest.h>
#include "common/allocator_internal.h"
#include "bztree.h"
#include "include/pmwcas.h"
#include <random>

namespace pmwcas {
namespace test {

// most test patterns shamelessly borrowed from lab 3's BTree tests

// for testing: key len is 8 and val len is 8, so 10 keys is 16+16*10+8*10 = 256 bytes exactly
// then minus two for the free threshold
#define BZTREE_CAPACITY 8

struct SingleThreadTest {
  BzTree tree;

  SingleThreadTest() {
    MwCASMetrics::ThreadInitialize();
  }

  ~SingleThreadTest() {
    tree.destroy();
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
}

GTEST_TEST(BzTreeTest, CompactSingleLeaf) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  t->tree.insert("always kept", "safe and sound");

  // add and remove 3x capacity
  for (auto i = 0; i < BZTREE_CAPACITY*3; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    t->tree.insert(kid, vid);
    ASSERT_TRUE(t->tree.lookup(kid))
        << "searching for the just inserted key k=" << kid << " yields nothing";
    t->tree.erase(kid);
    ASSERT_FALSE(t->tree.lookup(kid))
        << "searching for the just erased key k=" << kid << " yields something";
  }
}

GTEST_TEST(BzTreeTest, LookupSingleSplit) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  // Fill one page and a bit
  for (auto i = 0; i < BZTREE_CAPACITY + 3; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    t->tree.insert(kid, vid);

    ASSERT_TRUE(t->tree.lookup(kid))
        << "searching for the just inserted key k=" << kid << " yields nothing";
  }


  // Lookup all values
  for (auto i = 0; i < BZTREE_CAPACITY + 3; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    auto v = t->tree.lookup(kid);
    ASSERT_TRUE(v) << "key=" << kid << " is missing";
    ASSERT_TRUE(v == vid) << "key=" << kid << " wrong value";
  }
}

GTEST_TEST(BzTreeTest, LookupMultiLevelSplit) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  for (auto i = 0; i < BZTREE_CAPACITY*10 ; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    t->tree.insert(kid, vid);

    ASSERT_TRUE(t->tree.lookup(kid))
        << "searching for the just inserted key k=" << kid << " yields nothing";
  }

  // Lookup all values
  for (auto i = 0; i < BZTREE_CAPACITY*10; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    auto v = t->tree.lookup(kid);
    ASSERT_TRUE(v) << "key=" << kid << " is missing";
    ASSERT_TRUE(v == vid) << "key=" << kid << " wrong value";
  }
}

GTEST_TEST(BzTreeTest, LookupMultiLevelSplitErase) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());

  for (auto i = 0; i < BZTREE_CAPACITY*10 ; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    t->tree.insert(kid, vid);

    ASSERT_TRUE(t->tree.lookup(kid))
        << "searching for the just inserted key k=" << kid << " yields nothing";
  }

  for (auto i = BZTREE_CAPACITY*2; i < BZTREE_CAPACITY*4 ; ++i) {
    std::string kid = _kid(i);
    t->tree.erase(kid);

    ASSERT_FALSE(t->tree.lookup(kid))
        << "searching for the just erased key k=" << kid << " yields something";
  }

  for (auto i = BZTREE_CAPACITY*2; i < BZTREE_CAPACITY*4 ; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    t->tree.insert(kid, vid);

    ASSERT_TRUE(t->tree.lookup(kid))
        << "searching for the just inserted key k=" << kid << " yields nothing";
  }

  // Lookup all values
  for (auto i = 0; i < BZTREE_CAPACITY*10; ++i) {
    std::string kid = _kid(i);
    std::string vid = _vid(i);
    auto v = t->tree.lookup(kid);
    ASSERT_TRUE(v) << "key=" << kid << " is missing";
    ASSERT_TRUE(v == vid) << "key=" << kid << " wrong value";
  }
}

GTEST_TEST(BzTreeTest, LookupRandomNonRepeating) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());
  auto n = 10 * BZTREE_CAPACITY;

  // Generate random non-repeating key sequence
  std::vector<uint64_t> keys(n);
  std::iota(keys.begin(), keys.end(), n);
  std::mt19937_64 engine(0);
  std::shuffle(keys.begin(), keys.end(), engine);

  // Insert values
  for (auto i = 0ul; i < n; ++i) {
    t->tree.insert(_kid(keys[i]), _vid(2 * keys[i]));
    ASSERT_TRUE(t->tree.lookup(_kid(keys[i])))
        << "searching for the just inserted key k=" << _kid(keys[i])
        << " after i=" << i << " inserts yields nothing";
  }

  // Lookup all values
  for (auto i = 0ul; i < n; ++i) {
    auto v = t->tree.lookup(_kid(keys[i]));
    ASSERT_TRUE(v) << "key=" << _kid(keys[i]) << " is missing";
    ASSERT_TRUE(*v == _vid(2 * keys[i]))
        << "key=" << _kid(keys[i]) << " should have the value v=" << _vid(2 * keys[i]);
  }
}

GTEST_TEST(BzTreeTest, LookupRandomRepeating) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());
  auto n = 10 * BZTREE_CAPACITY;

  // Insert & updated 100 keys at random
  std::mt19937_64 engine{0};
  std::uniform_int_distribution<uint64_t> key_distr(0, 99);
  std::vector<uint64_t> values(100);

  for (auto i = 1ul; i < n; ++i) {
    uint64_t rand_key = key_distr(engine);
    values[rand_key] = i;
    t->tree.erase(_kid(rand_key));
    t->tree.insert(_kid(rand_key), _vid(i));

    auto v = t->tree.lookup(_kid(rand_key));
    ASSERT_TRUE(v) << "searching for the just inserted key k=" << _kid(rand_key)
                   << " after i=" << (i - 1) << " inserts yields nothing";
    ASSERT_TRUE(*v == _vid(i)) << "overwriting k=" << _kid(rand_key) << " with value v=" << _vid(i)
                     << " failed";
  }

  // Lookup all values
  for (auto i = 0ul; i < 100; ++i) {
    if (values[i] == 0) {
      continue;
    }
    auto v = t->tree.lookup(_kid(i));
    ASSERT_TRUE(v) << "key=" << _kid(i) << " is missing";
    ASSERT_TRUE(*v == _vid(values[i]))
        << "key=" << _kid(i) << " should have the value v=" << _vid(values[i]);
  }
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
  unlink("bztree_test_pool");
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
