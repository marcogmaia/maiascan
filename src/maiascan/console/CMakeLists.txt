add_library(
  console
  "./console.cpp"
  "./console.h"
)

target_link_libraries(console PUBLIC fmt::fmt PRIVATE CLI11::CLI11)
