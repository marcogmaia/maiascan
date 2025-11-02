# Copyright (c) Maia

add_library(core)

target_sources(
  core
  FILE_SET HEADERS
  i_memory_accessor.h
  memory_common.h
)
