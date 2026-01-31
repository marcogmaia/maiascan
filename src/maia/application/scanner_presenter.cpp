// Copyright (c) Maia

#include "maia/application/scanner_presenter.h"

namespace maia {

namespace {

/// \brief Internal tag type used for compile-time function pointer deduction.
template <auto Candidate>
struct SlotTag {};

/// \brief Compile-time constant wrapper for function pointers.
/// \details Usage: maia::Slot<&Class::Method> or maia::Slot<&FreeFunction>.
template <auto Candidate>
constexpr SlotTag<Candidate> Slot = {};  // NOLINT

/// \brief Connects a signal to a member function and manages connection
/// lifetime.
/// \param storage Container to hold the scoped connection (e.g., std::vector).
/// \param sink The source signal or sink.
/// \param instance The receiver object instance.
/// \param tag The slot wrapper (use maia::Slot<&Class::Method>).
template <typename Storage, typename Sink, typename Receiver, auto Candidate>
void Connect(Storage& storage,
             Sink&& sink,
             Receiver* instance,
             SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>(instance));
};

/// \brief Connects a signal to a free function or static method.
/// \param storage Container to hold the scoped connection.
/// \param sink The source signal or sink.
/// \param tag The slot wrapper (use maia::Slot<&Function>).
template <typename Storage, typename Sink, auto Candidate>
void Connect(Storage& storage, Sink&& sink, SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>());
}

enum GlobalHotkeyId {
  kHotkeyChanged = 1,
  kHotkeyUnchanged,
  kHotkeyIncreased,
  kHotkeyDecreased,
  kHotkeyExact,
  kHotkeyNextScan,
  kHotkeyNewScan
};

}  // namespace

ScannerPresenter::ScannerPresenter(ScanResultModel& scan_result_model,
                                   ProcessModel& process_model,
                                   CheatTableModel& cheat_table_model,
                                   ScannerWidget& scanner_widget,
                                   GlobalHotkeyManager& global_hotkey_manager)
    : scan_result_model_(scan_result_model),
      process_model_(process_model),
      cheat_table_model_(cheat_table_model),
      scanner_widget_(scanner_widget),
      global_hotkey_manager_(global_hotkey_manager) {
  // clang-format off

  // Connect ProcessModel to CheatTableModel 
  connections_.emplace_back(process_model_.sinks().ActiveProcessChanged().connect<&CheatTableModel::SetActiveProcess>(cheat_table_model_));
  connections_.emplace_back(process_model_.sinks().ActiveProcessChanged().connect<&ScanResultModel::SetActiveProcess>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().NewScanPressed().connect<&ScanResultModel::FirstScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().NextScanPressed().connect<&ScanResultModel::NextScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().ScanComparisonSelected().connect<&ScanResultModel::SetScanComparison>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().TargetValueSelected().connect<&ScanResultModel::SetTargetScanPattern>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().CancelScanPressed().connect<&ScanResultModel::CancelScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().ValueTypeSelected().connect<&ScanResultModel::SetScanValueType>(scan_result_model_));

  // connections_.emplace_back(scanner_widget_.sinks().AutoUpdateChanged().connect<&ScannerPresenter::OnAutoUpdateChanged>(*this));
  // I don't know if I'll keep this signature here, I'm just exploring ideas on how to mimic Qt's connection for signal/slow.
  Connect(connections_, scanner_widget_.sinks().AutoUpdateChanged(), this, Slot<&ScannerPresenter::OnAutoUpdateChanged>);
  Connect(connections_, scanner_widget_.sinks().PauseWhileScanningChanged(), this, Slot<&ScannerPresenter::OnPauseWhileScanningChanged>);
  Connect(connections_, scanner_widget_.sinks().FastScanChanged(), this, Slot<&ScannerPresenter::OnFastScanChanged>);
  Connect(connections_, scanner_widget_.sinks().EntryDoubleClicked(), this, Slot<&ScannerPresenter::OnEntryDoubleClicked>);

  // Register Global Hotkeys using cross-platform API
  using Key = KeyCode;
  using Mod = KeyModifier;
  const auto ctrl_shift = Mod::kControl | Mod::kShift;
  
  // Ctrl + Shift + C (Changed)
  global_hotkey_manager_.Register(kHotkeyChanged, ctrl_shift, Key::kC);
  // Ctrl + Shift + U (Unchanged)
  global_hotkey_manager_.Register(kHotkeyUnchanged, ctrl_shift, Key::kU);
  // Ctrl + Shift + + (Increased) - register both main keyboard and numpad
  global_hotkey_manager_.Register(kHotkeyIncreased, ctrl_shift, Key::kPlus);
  global_hotkey_manager_.Register(kHotkeyIncreased, ctrl_shift, Key::kNumpadAdd);
  // Ctrl + Shift + - (Decreased) - register both main keyboard and numpad
  global_hotkey_manager_.Register(kHotkeyDecreased, ctrl_shift, Key::kMinus);
  global_hotkey_manager_.Register(kHotkeyDecreased, ctrl_shift, Key::kNumpadSubtract);
  // Ctrl + Shift + E (Exact Value)
  global_hotkey_manager_.Register(kHotkeyExact, ctrl_shift, Key::kE);
  // Ctrl + Enter (Next Scan)
  global_hotkey_manager_.Register(kHotkeyNextScan, Mod::kControl, Key::kReturn);
  // Ctrl + N (New Scan)
  global_hotkey_manager_.Register(kHotkeyNewScan, Mod::kControl, Key::kN);

  Connect(connections_, global_hotkey_manager_.sinks().HotkeyTriggered(), this, Slot<&ScannerPresenter::OnGlobalHotkey>);

  // clang-format on
}

void ScannerPresenter::OnAutoUpdateChanged(bool is_checked) {
  if (is_checked) {
    scan_result_model_.StartAutoUpdate();
  } else {
    scan_result_model_.StopAutoUpdate();
  }
}

void ScannerPresenter::OnPauseWhileScanningChanged(bool is_checked) {
  scan_result_model_.SetPauseWhileScanning(is_checked);
}

void ScannerPresenter::OnFastScanChanged(bool is_checked) {
  scan_result_model_.SetFastScan(is_checked);
}

void ScannerPresenter::OnEntryDoubleClicked(int index, ScanValueType type) {
  // Get the address from the model
  const auto& results = scan_result_model_.entries();
  if (index >= 0 && static_cast<size_t>(index) < results.addresses.size()) {
    MemoryAddress address = results.addresses[index];
    // TODO: Get more meaningful description?
    std::string description = "No description";
    cheat_table_model_.AddEntry(address, type, description, results.stride);
  }
}

void ScannerPresenter::OnGlobalHotkey(int id) {
  switch (id) {
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
      // We don't auto-scan for Exact Value as it usually requires input
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
