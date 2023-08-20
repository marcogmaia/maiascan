add_library(scanner "./scanner.cpp" "./process.cpp" "./engine.cpp" "./scan.cpp")

target_compile_definitions(scanner PRIVATE "_AFXDLL")

target_link_libraries(scanner PUBLIC fmt::fmt tl::optional)

add_executable(scanner_test "./process_test.cpp" "./scan_test.cpp")
target_link_libraries(scanner_test PRIVATE scanner GTest::gmock_main)

gtest_discover_tests(scanner_test)
