# Copyright (c) Maia

find_package(EnTT CONFIG REQUIRED)

add_library(core)

target_sources(
  core
  PUBLIC
    FILE_SET HEADERS
      # BASE_DIRS maia/core
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        # i_memory_accessor.h
        # maia/core/
        cpu_info.h
        memory_common.h
        process.h
        simd_scanner.h
  PRIVATE
    process.cpp
)

target_link_libraries(
  core
  PUBLIC
    base
    EnTT::EnTT
    mmem
)

add_executable(simd_scanner_test simd_scanner_test.cpp)
target_link_libraries(
  simd_scanner_test
  PRIVATE
    core
    GTest::gmock_main
)

gtest_discover_tests(simd_scanner_test DISCOVERY_TIMEOUT 60)
