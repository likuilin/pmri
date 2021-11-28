#include <gtest/gtest.h>
#include "common/allocator_internal.h"
#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {
namespace test {

// most test patterns shamelessly borrowed from lab 3's BTree tests

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
                                                    static_cast<uint64_t>(1024) * 1024 * 1204 * 5),
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
