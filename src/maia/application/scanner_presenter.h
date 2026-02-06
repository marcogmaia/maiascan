// Copyright (c) Maia

#pragma once

#include "maia/application/cheat_table_model.h"
#include "maia/application/global_hotkey_manager.h"
#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/gui/widgets/scanner_view.h"

namespace maia {

class ScannerPresenter {
 public:
  ScannerPresenter(ScanResultModel& scan_result_model,
                   ProcessModel& process_model,
                   CheatTableModel& cheat_table_model,
                   ScannerWidget& scanner_widget,
                   GlobalHotkeyManager& global_hotkey_manager);

  void Render() {
    // Apply async scan results on the main thread when ready.
    if (scan_result_model_.HasPendingResult()) {
      scan_result_model_.ApplyPendingResult();
    }

    scanner_widget_.Render(scan_result_model_.entries(),
                           AddressFormatter(scan_result_model_.GetModules()),
                           scan_result_model_.GetProgress(),
                           scan_result_model_.IsScanning());
  }

  auto sinks() {
    return Sinks{*this};
  }

 private:
  class Signals {
   public:
    entt::sigh<void(uintptr_t)> browse_memory_requested;
  };

 public:
  struct Sinks {
    ScannerPresenter& presenter;

    auto BrowseMemoryRequested() {
      return entt::sink(presenter.signals_.browse_memory_requested);
    }
  };

 private:
  void OnAutoUpdateChanged(bool is_checked);
  void OnPauseWhileScanningChanged(bool is_checked);
  void OnFastScanChanged(bool is_checked);
  void OnEntryDoubleClicked(int index, ScanValueType type);
  void OnBrowseMemoryRequested(uintptr_t address) const;
  void OnGlobalHotkey(int id);

  ScanResultModel& scan_result_model_;
  ProcessModel& process_model_;
  CheatTableModel& cheat_table_model_;
  ScannerWidget& scanner_widget_;
  GlobalHotkeyManager& global_hotkey_manager_;

  Signals signals_;
  std::vector<entt::scoped_connection> connections_;
};

}  // namespace maia
