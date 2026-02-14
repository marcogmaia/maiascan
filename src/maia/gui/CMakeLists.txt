# Copyright (c) Maia

add_library(gui)

target_sources(
  gui
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS "${MAIA_BASE_DIR}"
      FILES
        imgui_effects.h
        imgui_extensions.h
        layout.h
        widgets/cheat_table_view.h
        widgets/data_inspector_view.h
        widgets/pointer_scanner_view.h
        widgets/process_selector_view.h
        widgets/results_table.h
        widgets/results_table_logic.h
        widgets/hex_view.h
        widgets/hex_cell_styler.h
        widgets/scanner_view.h
        models/hex_view_model.h
        models/ui_state.h
  PRIVATE
    imgui_effects.cpp
    imgui_extensions.cpp
    layout.cpp
    widgets/cheat_table_view.cpp
    widgets/data_inspector_view.cpp
    widgets/pointer_scanner_view.cpp
    widgets/process_selector_view.cpp
    widgets/results_table.cpp
    widgets/results_table_logic.cpp
    widgets/scanner_view.cpp
    widgets/hex_view.cpp
    widgets/hex_cell_styler.cpp
    models/hex_view_model.cpp
)

target_link_libraries(
  gui
  PUBLIC
    base
    imgui::imgui
    glfw
    glad::glad
)

target_compile_definitions(gui PUBLIC GLFW_INCLUDE_NONE)

add_executable(
  gui_test
  widgets/results_table_logic_test.cpp
  widgets/hex_cell_styler_test.cpp
)

target_link_libraries(
  gui_test
  PRIVATE
    gui
    GTest::gmock_main
)

gtest_discover_tests(gui_test DISCOVERY_TIMEOUT 60)
