// Copyright (c) Maia

#include "maia/application/scanner_viewmodel.h"

#include "maia/core/signal_utils.h"

namespace maia {

namespace {

enum class GlobalHotkeyId {
  kHotkeyChanged = 1,
  kHotkeyUnchanged,
  kHotkeyIncreased,
  kHotkeyDecreased,
  kHotkeyExact,
  kHotkeyNextScan,
  kHotkeyNewScan,
};

}  // namespace

ScannerViewModel::ScannerViewModel(ScanResultModel& scan_result_model,
                                   ProcessModel& process_model,
                                   CheatTableModel& cheat_table_model,
                                   GlobalHotkeyManager& global_hotkey_manager,
                                   gui::ScannerState& state)
    : scan_result_model_(scan_result_model),
      process_model_(process_model),
      cheat_table_model_(cheat_table_model),
      global_hotkey_manager_(global_hotkey_manager),
      state_(state) {
  // Model to Model connections
  Connect(connections_,
          process_model_.sinks().ActiveProcessChanged(),
          &cheat_table_model_,
          Slot<&CheatTableModel::SetActiveProcess>);
  Connect(connections_,
          process_model_.sinks().ActiveProcessChanged(),
          &scan_result_model_,
          Slot<&ScanResultModel::SetActiveProcess>);

  // Hotkey connection
  Connect(connections_,
          global_hotkey_manager_.sinks().HotkeyTriggered(),
          this,
          Slot<&ScannerViewModel::OnGlobalHotkey>);

  // Register Global Hotkeys
  using Key = KeyCode;
  using Mod = KeyModifier;
  const auto ctrl_shift = Mod::kControl | Mod::kShift;
  using enum GlobalHotkeyId;
  // clang-format off
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyChanged),   ctrl_shift,    Key::kC);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyUnchanged), ctrl_shift,    Key::kU);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyIncreased), ctrl_shift,    Key::kPlus);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyIncreased), ctrl_shift,    Key::kNumpadAdd);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyDecreased), ctrl_shift,    Key::kMinus);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyDecreased), ctrl_shift,    Key::kNumpadSubtract);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyExact),     ctrl_shift,    Key::kE);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyNextScan),  Mod::kControl, Key::kReturn);
  global_hotkey_manager_.Register(static_cast<int>(kHotkeyNewScan),   Mod::kControl, Key::kN);
  // clang-format on
}

void ScannerViewModel::Update() {
  if (scan_result_model_.HasPendingResult()) {
    scan_result_model_.ApplyPendingResult();
  }
  state_.progress = scan_result_model_.GetProgress();
  state_.is_scanning = scan_result_model_.IsScanning();
}

void ScannerViewModel::OnNewScanPressed() {
  scan_result_model_.FirstScan();
}

void ScannerViewModel::OnNextScanPressed() {
  scan_result_model_.NextScan();
}

void ScannerViewModel::OnCancelScanPressed() {
  scan_result_model_.CancelScan();
}

void ScannerViewModel::OnScanComparisonSelected(ScanComparison comparison) {
  scan_result_model_.SetScanComparison(comparison);
}

void ScannerViewModel::OnTargetValueSelected(std::vector<std::byte> value,
                                             std::vector<std::byte> mask) {
  scan_result_model_.SetTargetScanPattern(std::move(value), std::move(mask));
}

void ScannerViewModel::OnValueTypeSelected(ScanValueType type) {
  scan_result_model_.ChangeResultType(type);
}

void ScannerViewModel::OnAutoUpdateChanged(bool enabled) {
  if (enabled) {
    scan_result_model_.StartAutoUpdate();
  } else {
    scan_result_model_.StopAutoUpdate();
  }
}

void ScannerViewModel::OnPauseWhileScanningChanged(bool enabled) {
  scan_result_model_.SetPauseWhileScanning(enabled);
}

void ScannerViewModel::OnFastScanChanged(bool enabled) {
  scan_result_model_.SetFastScan(enabled);
}

void ScannerViewModel::OnEntryDoubleClicked(int index, ScanValueType type) {
  const auto& results = scan_result_model_.entries();
  if (index >= 0 && static_cast<size_t>(index) < results.addresses.size()) {
    cheat_table_model_.AddEntry(
        results.addresses[index], type, "No description", results.stride);
  }
}

void ScannerViewModel::OnReinterpretTypeRequested(ScanValueType type) {
  scan_result_model_.ChangeResultType(type);
}

// NOLINTNEXTLINE
void ScannerViewModel::OnBrowseMemoryRequested(uintptr_t address) {
  signals_.browse_memory_requested.publish(address);
}

void ScannerViewModel::OnGlobalHotkey(int id) {
  auto e = static_cast<GlobalHotkeyId>(id);
  switch (e) {
    using enum GlobalHotkeyId;
    case kHotkeyChanged:
      scan_result_model_.SetScanComparison(ScanComparison::kChanged);
      scan_result_model_.NextScan();
      break;
    case kHotkeyUnchanged:
      scan_result_model_.SetScanComparison(ScanComparison::kUnchanged);
      scan_result_model_.NextScan();
      break;
    case kHotkeyIncreased:
      scan_result_model_.SetScanComparison(ScanComparison::kIncreased);
      scan_result_model_.NextScan();
      break;
    case kHotkeyDecreased:
      scan_result_model_.SetScanComparison(ScanComparison::kDecreased);
      scan_result_model_.NextScan();
      break;
    case kHotkeyExact:
      scan_result_model_.SetScanComparison(ScanComparison::kExactValue);
      break;
    case kHotkeyNextScan:
      scan_result_model_.NextScan();
      break;
    case kHotkeyNewScan:
      scan_result_model_.FirstScan();
      break;
  }
}

}  // namespace maia
