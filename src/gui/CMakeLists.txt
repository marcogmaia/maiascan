# Copyright (c) Maia

add_library(
  gui
  imgui_extensions.cpp
  imgui_extensions.h
)

target_link_libraries(
  gui
  PUBLIC
    imgui::imgui
    glfw
    scanner
    console
    glad::glad
)

target_compile_definitions(gui PUBLIC GLFW_INCLUDE_NONE)
