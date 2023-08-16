add_executable(console "./console.cpp" "./console.h")
# add_library(console "./console.cpp" "./console.h")

target_link_libraries(
  console
  PUBLIC fmt::fmt
  PRIVATE cxxopts::cxxopts cli::cli)
