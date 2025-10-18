# Copyright (c) Maia

add_executable(
  maiascan
  main.cpp
  imgui_extensions.cpp
)

target_link_libraries(
  maiascan
  PRIVATE
    imgui::imgui
    glfw
    scanner
    console
    glad::glad
)

target_compile_definitions(maiascan PRIVATE GLFW_INCLUDE_NONE)
