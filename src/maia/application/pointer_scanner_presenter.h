// Copyright (c) Maia

#pragma once

#include "maia/application/cheat_table_model.h"
#include "maia/application/pointer_scanner_model.h"
#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/application/throttled_value_cache.h"
#include "maia/gui/widgets/pointer_scanner_view.h"

namespace maia {

/// \brief Coordinates between PointerScannerModel and PointerScannerView.
/// \details Manages signal connections, handles async result processing,
/// and coordinates data flow for the pointer scanner feature.
class PointerScannerPresenter {
 public:
  PointerScannerPresenter(PointerScannerModel& pointer_scanner_model,
                          ProcessModel& process_model,
                          CheatTableModel& cheat_table_model,
                          ScanResultModel& scan_result_model,
                          PointerScannerView& pointer_scanner_view);

  /// \brief Render the pointer scanner window if visible.
  void Render();

  /// \brief Show or hide the pointer scanner window.
  void SetVisible(bool visible) {
    is_visible_ = visible;
  }

  /// \brief Check if the window is currently visible.
  [[nodiscard]] bool IsVisible() const {
    return is_visible_;
  }

  /// \brief Toggle window visibility.
  void ToggleVisibility() {
    is_visible_ = !is_visible_;
  }

 private:
  // View signal handlers
  void OnTargetAddressChanged(uint64_t address);
  void OnTargetTypeChanged(ScanValueType type);
  void OnTargetFromCheatSelected(size_t index);
  void OnTargetFromScanSelected(size_t index);
  void OnGenerateMapPressed();
  void OnSaveMapPressed();
  void OnLoadMapPressed();
  void OnFindPathsPressed();
  void OnValidatePressed();
  void OnCancelPressed();
  void OnResultDoubleClicked(size_t index);

  // Model signal handlers
  void OnScanComplete(const core::PointerScanResult& result);
  void OnProgressUpdated(float progress, const std::string& operation);
  void OnPathsUpdated();
  void OnValidationComplete(const std::vector<core::PointerPath>& valid_paths);

  // Helper methods
  void UpdateTargetFromCheatTable(size_t index);
  void UpdateTargetFromScanResults(size_t index);
  void AddPathToCheatTable(size_t index);
  void HandlePendingProcessSwitch();
  void OnActiveProcessChanged(IProcess* process);
  void OnProcessWillDetach();

  PointerScannerModel& pointer_scanner_model_;
  ProcessModel& process_model_;
  CheatTableModel& cheat_table_model_;
  ScanResultModel& scan_result_model_;
  PointerScannerView& pointer_scanner_view_;

  bool is_visible_ = false;
  IProcess* pending_process_switch_ = nullptr;
  std::vector<entt::scoped_connection> connections_;

  // Value cache for throttled memory reading (100ms refresh)
  ThrottledValueCache value_cache_;
};

}  // namespace maia
