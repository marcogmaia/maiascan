# Copyright (c) Maia

add_library(application)

target_sources(
  application
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        memory_scanner.h
        process_model.h
        process_selector_presenter.h
        scanner_presenter.h
        scan_result_model.h
  PRIVATE
    memory_scanner.cpp
    process_selector_presenter.cpp
    scanner_presenter.cpp
    scan_result_model.cpp
)

target_link_libraries(application PUBLIC core)

add_executable(memory_scanner_test scan_result_model_test.cpp)
target_link_libraries(
  memory_scanner_test
  PRIVATE
    application
    GTest::gmock_main
)

gtest_discover_tests(memory_scanner_test DISCOVERY_TIMEOUT 60)
