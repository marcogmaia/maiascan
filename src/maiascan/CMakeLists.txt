# Copyright (c) Maia

add_subdirectory(console)
add_subdirectory(scanner)

add_executable(maiascan main.cpp)
target_link_libraries(
  maiascan
  PRIVATE
    gui
    scanner
)
