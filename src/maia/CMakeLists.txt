# Copyright (c) Maia

add_subdirectory(application)
add_subdirectory(console)
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(scanner)

find_package(spdlog CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(libassert CONFIG REQUIRED)

add_library(base)
target_sources(
  base
  PUBLIC
    FILE_SET HEADERS
      # TODO: Fix this name "assert2".
      BASE_DIRS .
      FILES logging.h assert2.h
  PRIVATE
    assert.cpp
)
target_link_libraries(
  base
  PUBLIC
    spdlog::spdlog
    EnTT::EnTT
    libassert::assert
)

add_executable(maiascan main.cpp)
target_link_libraries(
  maiascan
  PRIVATE
    base
    gui
    core
    application
    scanner
)
