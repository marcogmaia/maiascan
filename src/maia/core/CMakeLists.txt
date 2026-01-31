# Copyright (c) Maia

find_package(EnTT CONFIG REQUIRED)

add_library(core)

target_sources(
  core
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        address_formatter.h
        cpu_info.h
        memory_common.h
        process.h
        pattern_parser.h
        pointer_map.h
        pointer_scanner.h
        scan_config.h
        scan_session.h
        scanner.h
        simd_scanner.h
        scoped_process_suspend.h
  PRIVATE
    address_formatter.cpp
    cpu_info.cpp
    process.cpp
    pattern_parser.cpp
    pointer_map.cpp
    pointer_scanner.cpp
    scanner.cpp
    simd_scanner.cpp
)

target_link_libraries(
  core
  PUBLIC
    base
    EnTT::EnTT
    mmem
)

add_executable(
  core_test
  address_formatter_test.cpp
  pattern_parser_test.cpp
  pointer_map_test.cpp
  pointer_scanner_test.cpp
  simd_scanner_test.cpp
  value_parser_test.cpp
)

target_link_libraries(
  core_test
  PRIVATE
    core
    test_utils
    GTest::gmock_main
)

gtest_discover_tests(core_test DISCOVERY_TIMEOUT 60)
