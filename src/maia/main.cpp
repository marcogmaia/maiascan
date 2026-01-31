// Copyright (c) Maia

// #include <numeric>

#include <fmt/core.h>
#include <imgui.h>
#include <entt/signal/dispatcher.hpp>

#include "application/scanner_presenter.h"
#include "maia/application/cheat_table_model.h"
#include "maia/application/cheat_table_presenter.h"
#include "maia/application/global_hotkey_manager.h"
#include "maia/application/pointer_scanner_model.h"
#include "maia/application/pointer_scanner_presenter.h"
#include "maia/application/process_selector_presenter.h"
#include "maia/application/scan_result_model.h"
#include "maia/gui/imgui_extensions.h"
#include "maia/gui/layout.h"
#include "maia/gui/widgets/cheat_table_view.h"
#include "maia/gui/widgets/pointer_scanner_view.h"
#include "maia/gui/widgets/process_selector_view.h"
#include "maia/gui/widgets/scanner_view.h"
#include "maia/logging.h"

namespace maia {

namespace {

void CreateDockSpace() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags host_window_flags = 0;
  host_window_flags |= ImGuiWindowFlags_NoTitleBar;
  host_window_flags |= ImGuiWindowFlags_NoCollapse;
  host_window_flags |= ImGuiWindowFlags_NoResize;
  host_window_flags |= ImGuiWindowFlags_NoMove;
  host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
  host_window_flags |= ImGuiWindowFlags_NoNavFocus;
  host_window_flags |= ImGuiWindowFlags_NoBackground;  // Make it transparent

  // We must push style vars to remove padding/borders
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("MaiaScan Host", nullptr, host_window_flags);

  ImGui::PopStyleVar(3);

  // Create the dockspace.
  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

  // Apply default layout if needed
  gui::MakeDefaultLayout(dockspace_id);
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
  ImGui::End();
}

}  // namespace

}  // namespace maia

int main() {
  maia::LogInstallFormat();

  maia::GuiSystem gui_system{};
  if (!gui_system.IsValid()) {
    maia::LogError("Failed to initialize the windowing subsystem.");
    return EXIT_FAILURE;
  }

  ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);

  maia::ProcessModel process_model{};
  maia::ProcessSelectorView proc_selector_view{};
  maia::ProcessSelectorPresenter process_selector{process_model,
                                                  proc_selector_view};

  maia::ScanResultModel scan_result_model{};
  maia::CheatTableModel cheat_table_model{};

  // Create global hotkey manager
  auto hotkey_manager =
      maia::GlobalHotkeyManager::Create(gui_system.window_handle());

  maia::ScannerWidget scanner_widget{};
  maia::ScannerPresenter scanner{scan_result_model,
                                 process_model,
                                 cheat_table_model,
                                 scanner_widget,
                                 *hotkey_manager};

  maia::CheatTableView cheat_table_view{};
  maia::CheatTablePresenter cheat_table{cheat_table_model, cheat_table_view};

  // Pointer Scanner
  maia::PointerScannerModel pointer_scanner_model{};
  maia::PointerScannerView pointer_scanner_view{};
  maia::PointerScannerPresenter pointer_scanner{pointer_scanner_model,
                                                process_model,
                                                cheat_table_model,
                                                scan_result_model,
                                                pointer_scanner_view};

  while (!gui_system.WindowShouldClose()) {
    gui_system.PollEvents();

    // Poll global hotkeys
    if (hotkey_manager) {
      hotkey_manager->Poll();
    }

    gui_system.BeginFrame();

    // Main menu bar
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("Tools")) {
        bool is_open = pointer_scanner.IsVisible();
        if (ImGui::MenuItem("Pointer Scanner", "Ctrl+Shift+P", &is_open)) {
          pointer_scanner.SetVisible(is_open);
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    // Keyboard shortcut for pointer scanner
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P)) {
      pointer_scanner.ToggleVisibility();
    }

    maia::CreateDockSpace();

    process_selector.Render();
    scanner.Render();
    cheat_table.Render();
    pointer_scanner.Render();

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
