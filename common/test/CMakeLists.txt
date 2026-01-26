
#set(MODULEPATH ${CMAKE_SOURCE_DIR}/libgnucash/core-utils)

set(COMMON_TEST_INCLUDE_DIRS
  ${CMAKE_BINARY_DIR}/common
  ${CMAKE_SOURCE_DIR}/common
)

set(COMMON_TEST_LIBS gtest)

gnc_add_test(test-except-fence "test-except-fence.cpp"
  COMMON_TEST_INCLUDE_DIRS COMMON_TEST_LIBS)

