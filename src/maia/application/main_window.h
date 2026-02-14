// Copyright (c) Maia

#pragma once

#include <entt/entt.hpp>
#include <vector>

#include "maia/application/cheat_table_viewmodel.h"
#include "maia/application/hex_view_viewmodel.h"
#include "maia/application/pointer_scanner_viewmodel.h"
#include "maia/application/process_selector_viewmodel.h"
#include "maia/application/scanner_viewmodel.h"
#include "maia/gui/models/ui_state.h"
#include "maia/gui/widgets/cheat_table_view.h"
#include "maia/gui/widgets/hex_view.h"
#include "maia/gui/widgets/pointer_scanner_view.h"
#include "maia/gui/widgets/process_selector_view.h"
#include "maia/gui/widgets/scanner_view.h"

namespace maia {

class MainWindow {
 public:
  MainWindow(ProcessSelectorViewModel& process_selector_vm,
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
             PointerScannerModel& pointer_scanner_model);

  void Render();

 private:
  void CreateDockSpace();
  void RenderMenuBar();

  // ViewModels
  ProcessSelectorViewModel& process_selector_vm_;
  ScannerViewModel& scanner_vm_;
  CheatTableViewModel& cheat_table_vm_;
  PointerScannerViewModel& pointer_scanner_vm_;
  HexViewViewModel& hex_vm_;

  // Models (for direct data access in Views if needed, or keeping references)
  ScanResultModel& scan_result_model_;
  CheatTableModel& cheat_table_model_;
  PointerScannerModel& pointer_scanner_model_;
  gui::HexViewModel& hex_view_model_;

  // UI States
  gui::ProcessSelectorState& process_selector_state_;
  gui::ScannerState& scanner_state_;
  gui::CheatTableState& cheat_table_state_;
  gui::PointerScannerState& pointer_scanner_state_;

  // Views (owned by MainWindow)
  ProcessSelectorView process_selector_view_;
  ScannerWidget scanner_view_;
  CheatTableView cheat_table_view_;
  PointerScannerView pointer_scanner_view_;
  gui::HexView hex_view_;

  std::vector<entt::scoped_connection> connections_;
};

}  // namespace maia
