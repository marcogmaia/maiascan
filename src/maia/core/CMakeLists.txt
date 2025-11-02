# Copyright (c) Maia

find_package(EnTT CONFIG REQUIRED)

add_library(core INTERFACE)

target_sources(
  core
  INTERFACE
    FILE_SET HEADERS
      FILES i_memory_accessor.h memory_common.h
)

target_link_libraries(
  core
  INTERFACE
    base
    EnTT::EnTT
)
