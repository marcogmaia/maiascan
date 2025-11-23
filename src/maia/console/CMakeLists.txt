# Copyright (c) Maia

add_library(
  console
  console.cpp
  console.h
)

target_sources(
  console
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES console.h
  PRIVATE
    console.cpp
)

target_link_libraries(console PUBLIC fmt::fmt PRIVATE CLI11::CLI11)
