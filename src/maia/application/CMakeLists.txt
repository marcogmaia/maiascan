# Copyright (c) Maia

find_package(GTest CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)

add_library(application)

target_sources(
  application
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        cheat_table_model.h
        cheat_table_presenter.h
        pointer_scanner_model.h
        pointer_scanner_presenter.h
        process_model.h
        process_selector_presenter.h
        scanner_presenter.h
        scan_result_model.h
        global_hotkey_manager.h
        hex_view_presenter.h
  PRIVATE
    cheat_table_model.cpp
    cheat_table_presenter.cpp
    global_hotkey_manager.cpp
    $<$<PLATFORM_ID:Windows>:global_hotkey_manager_win32.cpp>
    pointer_scanner_model.cpp
    pointer_scanner_presenter.cpp
    process_selector_presenter.cpp
    scanner_presenter.cpp
    scan_result_model.cpp
    hex_view_presenter.cpp
)

target_link_libraries(
  application
  PUBLIC
    core
    nfd::nfd
)

add_executable(application_test)
target_sources(
  application_test
  PRIVATE
    scan_result_model_test.cpp
    cheat_table_model_test.cpp
    pointer_scanner_model_test.cpp
)
target_link_libraries(
  application_test
  PRIVATE
    application
    test_utils
    GTest::gmock_main
)

gtest_discover_tests(application_test)
