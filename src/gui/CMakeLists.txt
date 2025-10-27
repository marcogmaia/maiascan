# Copyright (c) Maia

add_library(
  gui
  imgui_extensions.cpp
  imgui_extensions.h
  # Widgets
  widgets/process_picker.cpp
  widgets/process_picker.h
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

target_compile_definitions(
  gui
  PUBLIC
    GLFW_INCLUDE_NONE
  # GLFW_EXPOSE_NATIVE_WIN32
)
