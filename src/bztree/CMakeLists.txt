set(UTIL_HEADERS
  bztree.h
)

set(UTIL_SOURCES
  bztree.cc
)

add_library(bztree STATIC ${UTIL_SOURCES} ${UTIL_HEADERS})
target_link_libraries(bztree ${GFLAGS_LIB} ${GLOG_LIB})
target_compile_features(bztree PRIVATE cxx_std_17)

ADD_PMWCAS_TEST(bztree_tests)
target_compile_features(bztree_tests PRIVATE cxx_std_17)
