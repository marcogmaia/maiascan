# Copyright (c) Maia

find_package(EnTT CONFIG REQUIRED)

add_library(core INTERFACE)

target_sources(
  core
  INTERFACE
    FILE_SET HEADERS
      FILES i_memory_accessor.h memory_common.h
)

target_link_libraries(
  core
  INTERFACE
    base
    EnTT::EnTT
)

add_executable(core_test scan_result_test.cpp)
target_link_libraries(
  core_test
  PRIVATE
    core
    GTest::gmock_main
)

gtest_discover_tests(core_test DISCOVERY_TIMEOUT 60)
