add_library(console "./console.cpp" "./console.h")
# add_library(console "./console.cpp" "./console.h")

target_link_libraries(
  console
  PUBLIC fmt::fmt tl::expected tl::optional
  PRIVATE cli::cli CLI11::CLI11)
