// Copyright (c) Maia

#pragma once

#include <entt/entt.hpp>
#include <vector>

#include "maia/application/cheat_table_model.h"
#include "maia/application/global_hotkey_manager.h"
#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/gui/models/ui_state.h"

namespace maia {

class ScannerViewModel {
 public:
  ScannerViewModel(ScanResultModel& scan_result_model,
                   ProcessModel& process_model,
                   CheatTableModel& cheat_table_model,
                   GlobalHotkeyManager& global_hotkey_manager,
                   gui::ScannerState& state);

  void Update();

  // Slots for View signals
  void OnNewScanPressed();
  void OnNextScanPressed();
  void OnCancelScanPressed();
  void OnScanComparisonSelected(ScanComparison comparison);
  void OnTargetValueSelected(std::vector<std::byte> value,
                             std::vector<std::byte> mask);
  void OnValueTypeSelected(ScanValueType type);
  void OnAutoUpdateChanged(bool enabled);
  void OnPauseWhileScanningChanged(bool enabled);
  void OnFastScanChanged(bool enabled);
  void OnEntryDoubleClicked(int index, ScanValueType type);
  void OnReinterpretTypeRequested(ScanValueType type);
  void OnBrowseMemoryRequested(uintptr_t address);

  auto sinks() {
    return Sinks{*this};
  }

  struct Sinks {
    ScannerViewModel& vm;

    auto BrowseMemoryRequested() {
      return entt::sink(vm.signals_.browse_memory_requested);
    }
  };

 private:
  void OnGlobalHotkey(int id);

  ScanResultModel& scan_result_model_;
  ProcessModel& process_model_;
  CheatTableModel& cheat_table_model_;
  GlobalHotkeyManager& global_hotkey_manager_;
  gui::ScannerState& state_;

  struct Signals {
    entt::sigh<void(uintptr_t)> browse_memory_requested;
  };

  Signals signals_;
  std::vector<entt::scoped_connection> connections_;
};

}  // namespace maia
