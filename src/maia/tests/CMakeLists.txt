# Copyright (c) Maia

add_library(test_utils)

target_sources(
  test_utils
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES fake_process.h task_runner.h
  PRIVATE
    fake_process.cpp
)

target_link_libraries(
  test_utils
  PUBLIC
    core
    base
)

add_executable(hex_view_model_test hex_view_model_test.cpp)

target_link_libraries(
  hex_view_model_test
  PRIVATE
    gui
    test_utils
    GTest::gmock_main
)

gtest_discover_tests(hex_view_model_test DISCOVERY_TIMEOUT 60)
