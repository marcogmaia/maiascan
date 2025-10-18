# Copyright (c) Maia

# find_package()

set(CMAKE_MFC_FLAG 2)

add_library(
  scanner
  "./scanner.cpp"
  "./process.cpp"
  "./engine.cpp"
  "./scan.cpp"
)

target_compile_definitions(scanner PRIVATE _AFXDLL)
target_link_libraries(scanner PRIVATE fmt::fmt)

add_executable(
  scanner_test
  "./process_test.cpp"
  "./scan_test.cpp"
)
target_link_libraries(
  scanner_test
  PRIVATE
    scanner
    GTest::gmock_main
    fmt::fmt
)

gtest_discover_tests(scanner_test)
