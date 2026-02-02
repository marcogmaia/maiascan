# Copyright (c) Maia

add_library(mmem)

target_sources(
  mmem
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES mmem.h process_utils.h
  PRIVATE
    mmem.cpp
    process_utils.cpp
)

target_compile_definitions(mmem PUBLIC NOMINMAX)
