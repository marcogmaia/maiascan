// Copyright (c) Maia

#include <fmt/core.h>
#include <imgui.h>
#include <entt/signal/dispatcher.hpp>

#include "maia/application/cheat_table_model.h"
#include "maia/application/cheat_table_viewmodel.h"
#include "maia/application/global_hotkey_manager.h"
#include "maia/application/hex_view_viewmodel.h"
#include "maia/application/main_window.h"
#include "maia/application/pointer_scanner_model.h"
#include "maia/application/pointer_scanner_viewmodel.h"
#include "maia/application/process_selector_viewmodel.h"
#include "maia/application/scan_result_model.h"
#include "maia/application/scanner_viewmodel.h"
#include "maia/gui/imgui_extensions.h"
#include "maia/gui/models/hex_view_model.h"
#include "maia/gui/models/ui_state.h"
#include "maia/logging.h"

int main() {
  maia::LogInstallFormat();

  maia::GuiSystem gui_system{1440, 900};
  if (!gui_system.IsValid()) {
    maia::LogError("Failed to initialize the windowing subsystem.");
    return EXIT_FAILURE;
  }

  ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);

  // Models.
  maia::ProcessModel process_model{};
  maia::ScanResultModel scan_result_model{};
  maia::CheatTableModel cheat_table_model{};
  maia::PointerScannerModel pointer_scanner_model{};
  maia::gui::HexViewModel hex_view_model{};

  // Create global hotkey manager.
  auto hotkey_manager =
      maia::GlobalHotkeyManager::Create(gui_system.window_handle());

  // UI States.
  maia::gui::ProcessSelectorState process_selector_state{};
  maia::gui::ScannerState scanner_state{};
  maia::gui::CheatTableState cheat_table_state{};
  maia::gui::PointerScannerState pointer_scanner_state{};

  // ViewModels.
  maia::ProcessSelectorViewModel process_selector_vm{process_model,
                                                     process_selector_state};

  maia::ScannerViewModel scanner_vm{scan_result_model,
                                    process_model,
                                    cheat_table_model,
                                    *hotkey_manager,
                                    scanner_state};

  maia::CheatTableViewModel cheat_table_vm{
      cheat_table_model, process_model, cheat_table_state};

  maia::PointerScannerViewModel pointer_scanner_vm{pointer_scanner_model,
                                                   process_model,
                                                   cheat_table_model,
                                                   scan_result_model,
                                                   pointer_scanner_state};

  maia::HexViewViewModel hex_vm{process_model, hex_view_model};

  maia::MainWindow main_window{process_selector_vm,
                               process_selector_state,
                               scanner_vm,
                               scanner_state,
                               cheat_table_vm,
                               cheat_table_state,
                               pointer_scanner_vm,
                               pointer_scanner_state,
                               hex_vm,
                               hex_view_model,
                               scan_result_model,
                               cheat_table_model,
                               pointer_scanner_model};

  while (!gui_system.WindowShouldClose()) {
    gui_system.PollEvents();

    // Poll global hotkeys.
    if (hotkey_manager) {
      hotkey_manager->Poll();
    }

    gui_system.BeginFrame();

    main_window.Render();

    gui_system.ClearWindow(clear_color.x * clear_color.w,
                           clear_color.y * clear_color.w,
                           clear_color.z * clear_color.w,
                           clear_color.w);
    gui_system.EndFrame();
    gui_system.SwapBuffers();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  return 0;
}
