# Copyright (c) Maia

add_library(gui)

target_sources(
  gui
  PUBLIC
    FILE_SET HEADERS
      FILES
        imgui_extensions.h
        widgets/process_selector_view.h
        widgets/scanner_view.h
  PRIVATE
    imgui_extensions.cpp
    widgets/process_selector_view.cpp
    widgets/scanner_view.cpp
)

target_link_libraries(
  gui
  PUBLIC
    base
    imgui::imgui
    glfw
    console
    glad::glad
)

target_compile_definitions(gui PUBLIC GLFW_INCLUDE_NONE)
