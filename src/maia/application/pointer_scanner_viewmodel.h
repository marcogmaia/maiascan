// Copyright (c) Maia

#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>

#include "maia/application/cheat_table_model.h"
#include "maia/application/pointer_scanner_model.h"
#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/application/throttled_value_cache.h"
#include "maia/gui/models/ui_state.h"

namespace maia {

class PointerScannerViewModel {
 public:
  PointerScannerViewModel(PointerScannerModel& pointer_scanner_model,
                          ProcessModel& process_model,
                          CheatTableModel& cheat_table_model,
                          ScanResultModel& scan_result_model,
                          gui::PointerScannerState& state);

  void Update();

  // Slots for View signals
  void OnTargetAddressChanged(uint64_t address);
  void OnTargetTypeChanged(ScanValueType type);
  void OnTargetFromCheatSelected(size_t index);
  void OnTargetFromScanSelected(size_t index);
  void OnGenerateMapPressed();
  void OnSaveMapPressed();
  void OnLoadMapPressed();
  void OnFindPathsPressed(const core::PointerScanConfig& config);
  void OnValidatePressed();
  void OnCancelPressed();
  void OnResultDoubleClicked(size_t index);
  void OnShowAllPressed();

  // Data Provider logic (bridged by Binder)
  [[nodiscard]] std::optional<std::vector<std::byte>> GetValue(
      uintptr_t address);

  bool IsVisible() const {
    return state_.is_visible;
  }

  void SetVisible(bool visible) {
    state_.is_visible = visible;
  }

  void ToggleVisibility() {
    state_.is_visible = !state_.is_visible;
  }

 private:
  void OnProgressUpdated(float progress, const std::string& operation);
  void OnPathsUpdated();
  void OnValidationComplete(const std::vector<core::PointerPath>& valid_paths);
  void OnActiveProcessChanged(IProcess* process);

  PointerScannerModel& pointer_scanner_model_;
  ProcessModel& process_model_;
  CheatTableModel& cheat_table_model_;
  ScanResultModel& scan_result_model_;
  gui::PointerScannerState& state_;

  ThrottledValueCache value_cache_;

  std::vector<entt::scoped_connection> connections_;
};

}  // namespace maia
