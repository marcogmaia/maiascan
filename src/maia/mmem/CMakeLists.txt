# Copyright (c) Maia

add_library(mmem)

target_sources(
  mmem
  PUBLIC
    # proc.h
    # platform/win/proc.cpp
    mmem.cpp
    mmem.h
)

target_link_libraries(mmem PUBLIC base)

target_compile_definitions(mmem PUBLIC NOMINMAX)
