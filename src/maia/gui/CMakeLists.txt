# Copyright (c) Maia

add_library(gui)

target_sources(
  gui
  PUBLIC
    FILE_SET HEADERS
      FILES
        imgui_extensions.h
        widgets/process_picker.h
        widgets/process_selector.h
  PRIVATE
    imgui_extensions.cpp
    widgets/process_picker.cpp
    widgets/process_selector.cpp
)

target_link_libraries(
  gui
  PUBLIC
    base
    imgui::imgui
    glfw
    scanner
    console
    glad::glad
)

target_compile_definitions(gui PUBLIC GLFW_INCLUDE_NONE)
