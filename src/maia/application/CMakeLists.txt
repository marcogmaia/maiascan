# Copyright (c) Maia

find_package(GTest CONFIG REQUIRED)

add_library(application)

target_sources(
  application
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        cheat_table_model.h
        cheat_table_presenter.h
        process_model.h
        process_selector_presenter.h
        scanner_presenter.h
        scan_result_model.h
  PRIVATE
    cheat_table_model.cpp
    cheat_table_presenter.cpp
    process_selector_presenter.cpp
    scanner_presenter.cpp
    scan_result_model.cpp
)

target_link_libraries(application PUBLIC core)

add_executable(application_test)
target_sources(
  application_test
  PRIVATE
    scan_result_model_test.cpp
    cheat_table_model_test.cpp
)
target_link_libraries(
  application_test
  PRIVATE
    application
    GTest::gmock_main
)

gtest_discover_tests(application_test)
