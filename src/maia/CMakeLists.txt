# Copyright (c) Maia

add_subdirectory(application)
add_subdirectory(console)
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(scanner)

find_package(spdlog CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)

add_library(base INTERFACE)
target_sources(base INTERFACE FILE_SET HEADERS FILES logging.h)
target_link_libraries(
  base
  INTERFACE
    spdlog::spdlog
    EnTT::EnTT
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
