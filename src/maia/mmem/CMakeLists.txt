# Copyright (c) Maia

add_library(mmem)

target_sources(
  mmem
  PUBLIC
    FILE_SET HEADERS
      FILES mmem.h process_utils.h
  PRIVATE
    mmem.cpp
    process_utils.cpp
)

target_link_libraries(mmem PUBLIC base)

target_compile_definitions(mmem PUBLIC NOMINMAX)
