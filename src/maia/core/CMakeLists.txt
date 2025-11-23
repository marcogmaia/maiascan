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
        memory_common.h
        process.h
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

# add_executable(core_test scan_result_test.cpp)
# target_link_libraries(
#   core_test
#   PRIVATE
#     core
#     GTest::gmock_main
# )

# gtest_discover_tests(core_test DISCOVERY_TIMEOUT 60)
