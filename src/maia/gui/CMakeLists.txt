# Copyright (c) Maia

add_library(gui)

target_sources(
  gui
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        imgui_extensions.h
        layout.h
        widgets/cheat_table_view.h
        widgets/pointer_scanner_view.h
        widgets/process_selector_view.h
        widgets/results_table.h
        widgets/scanner_view.h
  PRIVATE
    imgui_extensions.cpp
    layout.cpp
    widgets/cheat_table_view.cpp
    widgets/pointer_scanner_view.cpp
    widgets/process_selector_view.cpp
    widgets/results_table.cpp
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
