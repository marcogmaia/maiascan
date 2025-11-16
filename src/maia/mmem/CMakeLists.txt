# Copyright (c) Maia

add_library(mmem)

target_sources(mmem PUBLIC FILE_SET HEADERS FILES mmem.h PRIVATE mmem.cpp)

target_link_libraries(mmem PUBLIC base)

target_compile_definitions(mmem PUBLIC NOMINMAX)
