# Copyright (c) Maia

add_executable(fakegame main.cpp)
target_link_libraries(
  fakegame
  fmt::fmt
  CLI11::CLI11
)
