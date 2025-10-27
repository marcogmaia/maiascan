# Copyright (c) Maia

set(CMAKE_MFC_FLAG 2)

add_library(
  scanner
  scanner.cpp
  process.cpp
  # engine.cpp
  # scan.cpp
)

target_compile_definitions(
  scanner
  PRIVATE
    _AFXDLL
    _WIN32_WINNT=0x0A00 # Target Windows 10
    WINVER=0x0A00 # Also set WINVER for compatibility
)

# target_compile_definitions(scanner PRIVATE _AFXDLL)
target_link_libraries(scanner PRIVATE fmt::fmt)

# add_executable(
#   scanner_test
#   process_test.cpp
#   # scan_test.cpp
# )
# target_link_libraries(
#   scanner_test
#   PRIVATE
#     scanner
#     GTest::gmock_main
#     fmt::fmt
# )

# gtest_discover_tests(scanner_test)
