add_library(console "./console.cpp" "./console.h")

target_link_libraries(
  console
  PUBLIC fmt::fmt tl::expected tl::optional
  PRIVATE CLI11::CLI11)
