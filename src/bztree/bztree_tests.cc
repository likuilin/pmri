#include <gtest/gtest.h>
#include "common/allocator_internal.h"
#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {
namespace test {

// most test patterns shamelessly borrowed from lab 3's BTree tests

#define BZTREE_MAX_NODES 10000

// root object contains descriptor pool and data
struct PMDKRootObj {
  DescriptorPool *desc_pool{nullptr};
  void *buf{nullptr};
};

// convert test uint64_t key to variable length vector<uint8_t> key
std::vector<uint8_t> convertKey(uint64_t k) {
  // undefined behavior by the way
  uint8_t *ptr = reinterpret_cast<uint8_t*>(&k);
  return std::vector<uint8_t>(ptr, ptr + 8);
}

struct SingleThreadTest {
  BzTree *tree;

  SingleThreadTest() {
    auto allocator = reinterpret_cast<PMDKAllocator*>(Allocator::Get());
    auto root_obj = reinterpret_cast<PMDKRootObj*>(allocator->GetRoot(sizeof(PMDKRootObj)));
    Allocator::Get()->Allocate((void **)&root_obj->desc_pool, sizeof(DescriptorPool));
    Allocator::Get()->Allocate(&root_obj->buf, BZTREE_NODE_SIZE*BZTREE_MAX_NODES);

    tree = new BzTree(root_obj->desc_pool, root_obj->buf, BZTREE_MAX_NODES);

    // MwCASMetrics::ThreadInitialize();
  }

  ~SingleThreadTest() {
    Thread::ClearRegistry();
  }

  void TestLookupEmptyTree() {
    auto test = "searching for a non-existing element in an empty B-Tree";
    ASSERT_FALSE(tree->lookup(convertKey(42))) << test << " seems to return something :-O";
  }
};

GTEST_TEST(BzTreeTest, LookupEmptyTree) {
  std::unique_ptr<SingleThreadTest> t(new SingleThreadTest());
  t->TestLookupEmptyTree();
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
