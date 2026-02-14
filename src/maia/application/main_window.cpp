// Copyright (c) Maia

#include "maia/application/main_window.h"

#include <imgui.h>

#include "maia/core/signal_utils.h"
#include "maia/gui/layout.h"

namespace maia {

namespace {

ImGuiWindowFlags GetHostWindowFlags() {
  ImGuiWindowFlags flags = 0;
  flags |= ImGuiWindowFlags_NoTitleBar;
  flags |= ImGuiWindowFlags_NoCollapse;
  flags |= ImGuiWindowFlags_NoResize;
  flags |= ImGuiWindowFlags_NoMove;
  flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
  flags |= ImGuiWindowFlags_NoNavFocus;
  flags |= ImGuiWindowFlags_NoBackground;
  return flags;
}

}  // namespace

MainWindow::MainWindow(ProcessSelectorViewModel& process_selector_vm,
                       gui::ProcessSelectorState& process_selector_state,
                       ScannerViewModel& scanner_vm,
                       gui::ScannerState& scanner_state,
                       CheatTableViewModel& cheat_table_vm,
                       gui::CheatTableState& cheat_table_state,
                       PointerScannerViewModel& pointer_scanner_vm,
                       gui::PointerScannerState& pointer_scanner_state,
                       HexViewViewModel& hex_vm,
                       gui::HexViewModel& hex_view_model,
                       ScanResultModel& scan_result_model,
                       CheatTableModel& cheat_table_model,
                       PointerScannerModel& pointer_scanner_model)
    : process_selector_vm_(process_selector_vm),
      scanner_vm_(scanner_vm),
      cheat_table_vm_(cheat_table_vm),
      pointer_scanner_vm_(pointer_scanner_vm),
      hex_vm_(hex_vm),
      scan_result_model_(scan_result_model),
      cheat_table_model_(cheat_table_model),
      pointer_scanner_model_(pointer_scanner_model),
      process_selector_state_(process_selector_state),
      scanner_state_(scanner_state),
      cheat_table_state_(cheat_table_state),
      pointer_scanner_state_(pointer_scanner_state),
      hex_view_model_(hex_view_model),
      process_selector_view_(),
      scanner_view_(),
      cheat_table_view_(),
      hex_view_(hex_view_model) {
  // clang-format off

  // ProcessSelector
  Connect(connections_, process_selector_view_.sinks().ProcessPickRequested(),      &process_selector_vm_, Slot<&ProcessSelectorViewModel::OnProcessPickRequested>);
  Connect(connections_, process_selector_view_.sinks().RefreshRequested(),          &process_selector_vm_, Slot<&ProcessSelectorViewModel::OnRefreshRequested>);
  Connect(connections_, process_selector_view_.sinks().ProcessSelectedFromList(),   &process_selector_vm_, Slot<&ProcessSelectorViewModel::AttachProcess>);

  // Scanner
  Connect(connections_, scanner_view_.sinks().NewScanPressed(),               &scanner_vm_, Slot<&ScannerViewModel::OnNewScanPressed>);
  Connect(connections_, scanner_view_.sinks().NextScanPressed(),              &scanner_vm_, Slot<&ScannerViewModel::OnNextScanPressed>);
  Connect(connections_, scanner_view_.sinks().CancelScanPressed(),            &scanner_vm_, Slot<&ScannerViewModel::OnCancelScanPressed>);
  Connect(connections_, scanner_view_.sinks().ScanComparisonSelected(),       &scanner_vm_, Slot<&ScannerViewModel::OnScanComparisonSelected>);
  Connect(connections_, scanner_view_.sinks().TargetValueSelected(),          &scanner_vm_, Slot<&ScannerViewModel::OnTargetValueSelected>);
  Connect(connections_, scanner_view_.sinks().ValueTypeSelected(),            &scanner_vm_, Slot<&ScannerViewModel::OnValueTypeSelected>);
  Connect(connections_, scanner_view_.sinks().AutoUpdateChanged(),            &scanner_vm_, Slot<&ScannerViewModel::OnAutoUpdateChanged>);
  Connect(connections_, scanner_view_.sinks().PauseWhileScanningChanged(),    &scanner_vm_, Slot<&ScannerViewModel::OnPauseWhileScanningChanged>);
  Connect(connections_, scanner_view_.sinks().FastScanChanged(),              &scanner_vm_, Slot<&ScannerViewModel::OnFastScanChanged>);
  Connect(connections_, scanner_view_.sinks().EntryDoubleClicked(),           &scanner_vm_, Slot<&ScannerViewModel::OnEntryDoubleClicked>);
  Connect(connections_, scanner_view_.sinks().ReinterpretTypeRequested(),     &scanner_vm_, Slot<&ScannerViewModel::OnReinterpretTypeRequested>);
  Connect(connections_, scanner_view_.sinks().BrowseMemoryRequested(),        &scanner_vm_, Slot<&ScannerViewModel::OnBrowseMemoryRequested>);
  Connect(connections_, scanner_vm_.sinks().BrowseMemoryRequested(),          &hex_vm_,    Slot<&HexViewViewModel::GoToAddress>);

  // CheatTable
  Connect(connections_, cheat_table_view_.sinks().FreezeToggled(),            &cheat_table_vm_, Slot<&CheatTableViewModel::OnFreezeToggled>);
  Connect(connections_, cheat_table_view_.sinks().DescriptionChanged(),       &cheat_table_vm_, Slot<&CheatTableViewModel::OnDescriptionChanged>);
  Connect(connections_, cheat_table_view_.sinks().HexDisplayToggled(),        &cheat_table_vm_, Slot<&CheatTableViewModel::OnHexDisplayToggled>);
  Connect(connections_, cheat_table_view_.sinks().ValueChanged(),             &cheat_table_vm_, Slot<&CheatTableViewModel::OnValueChanged>);
  Connect(connections_, cheat_table_view_.sinks().TypeChangeRequested(),      &cheat_table_vm_, Slot<&CheatTableViewModel::OnTypeChangeRequested>);
  Connect(connections_, cheat_table_view_.sinks().DeleteRequested(),          &cheat_table_vm_, Slot<&CheatTableViewModel::OnDeleteRequested>);
  Connect(connections_, cheat_table_view_.sinks().SaveRequested(),            &cheat_table_vm_, Slot<&CheatTableViewModel::OnSaveRequested>);
  Connect(connections_, cheat_table_view_.sinks().LoadRequested(),            &cheat_table_vm_, Slot<&CheatTableViewModel::OnLoadRequested>);
  Connect(connections_, cheat_table_view_.sinks().AddManualRequested(),       &cheat_table_vm_, Slot<&CheatTableViewModel::OnAddManualRequested>);

  // PointerScanner
  Connect(connections_, pointer_scanner_view_.sinks().TargetAddressChanged(),    &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnTargetAddressChanged>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetTypeChanged(),       &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnTargetTypeChanged>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetFromCheatSelected(), &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnTargetFromCheatSelected>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetFromScanSelected(),  &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnTargetFromScanSelected>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetAddressInvalid(),    &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnCancelPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().GenerateMapPressed(),      &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnGenerateMapPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().SaveMapPressed(),          &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnSaveMapPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().LoadMapPressed(),          &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnLoadMapPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().FindPathsPressed(),        &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnFindPathsPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().ValidatePressed(),         &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnValidatePressed>);
  Connect(connections_, pointer_scanner_view_.sinks().CancelPressed(),           &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnCancelPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().ResultDoubleClicked(),     &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnResultDoubleClicked>);
  Connect(connections_, pointer_scanner_view_.sinks().BrowseMemoryRequested(),   &hex_vm_,             Slot<&HexViewViewModel::GoToAddress>);
  Connect(connections_, pointer_scanner_view_.sinks().ShowAllPressed(),          &pointer_scanner_vm_, Slot<&PointerScannerViewModel::OnShowAllPressed>);

  // clang-format on
}

void MainWindow::Render() {
  // Update ViewModels
  scanner_vm_.Update();
  pointer_scanner_vm_.Update();

  RenderMenuBar();
  CreateDockSpace();

  // Render Views
  process_selector_view_.Render(process_selector_state_);

  scanner_view_.RenderControls(scanner_state_.progress,
                               scanner_state_.is_scanning);
  scanner_view_.RenderResults(
      scan_result_model_.entries(),
      AddressFormatter(scan_result_model_.GetModules()));

  cheat_table_view_.Render(*cheat_table_model_.entries());

  // Pointer Scanner
  bool ps_visible = pointer_scanner_vm_.IsVisible();
  pointer_scanner_view_.Render(
      &ps_visible,
      pointer_scanner_model_.GetPaths(),
      pointer_scanner_state_.map_entry_count,
      pointer_scanner_state_.map_progress,
      pointer_scanner_state_.scan_progress,
      pointer_scanner_state_.is_generating_map,
      pointer_scanner_state_.is_scanning,
      *cheat_table_model_.entries(),
      scan_result_model_.entries(),
      pointer_scanner_model_.GetModuleNames(),
      [this](const core::PointerPath& p) {
        return pointer_scanner_model_.ResolvePath(p);
      },
      [this](uint64_t addr) { return pointer_scanner_vm_.GetValue(addr); },
      pointer_scanner_state_.value_type,
      pointer_scanner_state_.show_all_results);
  pointer_scanner_vm_.SetVisible(ps_visible);

  if (hex_vm_.IsVisible()) {
    bool visible = true;
    if (ImGui::Begin("Memory Viewer", &visible)) {
      hex_view_.Render();
    }
    ImGui::End();

    if (!visible) {
      hex_vm_.SetVisible(false);
    }
  }
}

void MainWindow::CreateDockSpace() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags host_window_flags = GetHostWindowFlags();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("MaiaScan Host", nullptr, host_window_flags);

  ImGui::PopStyleVar(3);

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  gui::MakeDefaultLayout(dockspace_id);
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
  ImGui::End();
}

void MainWindow::RenderMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Tools")) {
      bool ps_open = pointer_scanner_vm_.IsVisible();
      if (ImGui::MenuItem("Pointer Scanner", "Ctrl+Shift+P", &ps_open)) {
        pointer_scanner_vm_.SetVisible(ps_open);
      }

      bool hex_open = hex_vm_.IsVisible();
      if (ImGui::MenuItem("Memory Viewer", "Ctrl+H", &hex_open)) {
        hex_vm_.SetVisible(hex_open);
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();
    if (RenderToolbar(process_selector_state_)) {
      process_selector_state_.is_visible = true;
    }

    ImGui::EndMainMenuBar();
  }

  // Handle keyboard shortcuts
  ImGuiIO& io = ImGui::GetIO();
  if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P)) {
    pointer_scanner_vm_.ToggleVisibility();
  }
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_H)) {
    hex_vm_.ToggleVisibility();
  }
}

}  // namespace maia
