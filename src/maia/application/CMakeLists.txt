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
        cheat_table_viewmodel.h
        main_window.h
        pointer_scanner_model.h
        pointer_scanner_viewmodel.h
        process_model.h
        process_selector_viewmodel.h
        scanner_viewmodel.h
        scan_result_model.h
        global_hotkey_manager.h
        hex_view_viewmodel.h
        file_dialogs.h
        throttled_value_cache.h
  PRIVATE
    cheat_table_model.cpp
    cheat_table_viewmodel.cpp
    global_hotkey_manager.cpp
    $<$<PLATFORM_ID:Windows>:global_hotkey_manager_win32.cpp>
    main_window.cpp
    pointer_scanner_model.cpp
    pointer_scanner_viewmodel.cpp
    process_selector_viewmodel.cpp
    scanner_viewmodel.cpp
    scan_result_model.cpp
    hex_view_viewmodel.cpp
    file_dialogs.cpp
    throttled_value_cache.cpp
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
    process_model_test.cpp
    throttled_value_cache_test.cpp
)
target_link_libraries(
  application_test
  PRIVATE
    application
    test_utils
    GTest::gmock_main
)

gtest_discover_tests(application_test)
