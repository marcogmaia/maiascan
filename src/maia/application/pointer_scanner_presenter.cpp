// Copyright (c) Maia

#include "maia/application/pointer_scanner_presenter.h"

#include <imgui.h>

#include "maia/logging.h"

namespace maia {

namespace {

/// \brief Internal tag type used for compile-time function pointer deduction.
template <auto Candidate>
struct SlotTag {};

/// \brief Compile-time constant wrapper for function pointers.
template <auto Candidate>
constexpr SlotTag<Candidate> Slot = {};  // NOLINT

/// \brief Connects a signal to a member function and manages connection
/// lifetime.
template <typename Storage, typename Sink, typename Receiver, auto Candidate>
void Connect(Storage& storage,
             Sink&& sink,
             Receiver* instance,
             SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>(instance));
};

/// \brief Connects a signal to a member function and manages connection
/// lifetime.
template <typename Storage, typename Sink, auto Candidate>
void Connect(Storage& storage, Sink&& sink, SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>());
};

void OnTargetAddressInvalid() {
  LogWarning("Invalid target address input");
}

void OnShowAllPressed() {
  // The view handles the display logic, we just log it
  LogDebug("User requested to show all pointer scan results");
}

void OnMapGenerated(bool success, size_t entry_count) {
  if (success) {
    LogInfo("Pointer map generated with {} entries", entry_count);
  } else {
    LogWarning("Pointer map generation failed");
  }
}

void OnScanComplete(const core::PointerScanResult& result) {
  if (result.success) {
    LogInfo("Pointer scan complete: {} paths found (evaluated: {})",
            result.paths.size(),
            result.paths_evaluated);
  } else {
    LogWarning("Pointer scan failed: {}", result.error_message);
  }
}

void OnSaveMapPressed() {
  // TODO(marco): Implement file dialog for saving pointer map
  // For now, log that the feature needs implementation
  LogWarning("Save map feature requires file dialog implementation");
}

void OnLoadMapPressed() {
  // TODO(marco): Implement file dialog for loading pointer map
  // For now, log that the feature needs implementation
  LogWarning("Load map feature requires file dialog implementation");
}

}  // namespace

PointerScannerPresenter::PointerScannerPresenter(
    PointerScannerModel& pointer_scanner_model,
    ProcessModel& process_model,
    CheatTableModel& cheat_table_model,
    ScanResultModel& scan_result_model,
    PointerScannerView& pointer_scanner_view)
    : pointer_scanner_model_(pointer_scanner_model),
      process_model_(process_model),
      cheat_table_model_(cheat_table_model),
      scan_result_model_(scan_result_model),
      pointer_scanner_view_(pointer_scanner_view) {
  // clang-format off
  // Connect process model signals to presenter handlers
  Connect(connections_, process_model_.sinks().ActiveProcessChanged(), this, Slot<&PointerScannerPresenter::OnActiveProcessChanged>);

  // Connect view signals to presenter handlers
  Connect(connections_, pointer_scanner_view_.sinks().TargetAddressChanged(),    this, Slot<&PointerScannerPresenter::OnTargetAddressChanged>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetTypeChanged(),       this, Slot<&PointerScannerPresenter::OnTargetTypeChanged>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetFromCheatSelected(), this, Slot<&PointerScannerPresenter::OnTargetFromCheatSelected>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetFromScanSelected(),  this, Slot<&PointerScannerPresenter::OnTargetFromScanSelected>);
  Connect(connections_, pointer_scanner_view_.sinks().GenerateMapPressed(),      this, Slot<&PointerScannerPresenter::OnGenerateMapPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().SaveMapPressed(),          Slot<OnSaveMapPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().LoadMapPressed(),          Slot<OnLoadMapPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().FindPathsPressed(),        this, Slot<&PointerScannerPresenter::OnFindPathsPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().ValidatePressed(),         this, Slot<&PointerScannerPresenter::OnValidatePressed>);
  Connect(connections_, pointer_scanner_view_.sinks().CancelPressed(),           this, Slot<&PointerScannerPresenter::OnCancelPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().ResultDoubleClicked(),     this, Slot<&PointerScannerPresenter::OnResultDoubleClicked>);
  Connect(connections_, pointer_scanner_view_.sinks().ShowAllPressed(),          Slot<OnShowAllPressed>);
  Connect(connections_, pointer_scanner_view_.sinks().TargetAddressInvalid(),    Slot<OnTargetAddressInvalid>);

  // Connect model signals to presenter handlers
  Connect(connections_, pointer_scanner_model_.sinks().MapGenerated(),       Slot<&OnMapGenerated>);
  Connect(connections_, pointer_scanner_model_.sinks().ScanComplete(),       Slot<&OnScanComplete>);
  Connect(connections_, pointer_scanner_model_.sinks().ProgressUpdated(),    this, Slot<&PointerScannerPresenter::OnProgressUpdated>);
  Connect(connections_, pointer_scanner_model_.sinks().PathsUpdated(),       this, Slot<&PointerScannerPresenter::OnPathsUpdated>);
  Connect(connections_, pointer_scanner_model_.sinks().ValidationComplete(), this, Slot<&PointerScannerPresenter::OnValidationComplete>);
  // clang-format on
}

void PointerScannerPresenter::Render() {
  // Handle pending process switch if not busy
  if (pending_process_switch_ && !pointer_scanner_model_.IsBusy()) {
    HandlePendingProcessSwitch();
  }

  // Get current data from models
  auto paths = pointer_scanner_model_.GetPaths();
  auto cheat_entries_ptr = cheat_table_model_.entries();
  const auto& cheat_entries = *cheat_entries_ptr;
  const auto& scan_results = scan_result_model_.entries();
  auto available_modules = pointer_scanner_model_.GetModuleNames();

  // Render the view with path resolver
  pointer_scanner_view_.Render(
      &is_visible_,
      paths,
      pointer_scanner_model_.GetMapEntryCount(),
      pointer_scanner_model_.GetMapProgress(),
      pointer_scanner_model_.GetScanProgress(),
      pointer_scanner_model_.IsGeneratingMap(),
      pointer_scanner_model_.IsScanning(),
      cheat_entries,
      scan_results,
      available_modules,
      [this](const core::PointerPath& path) {
        return pointer_scanner_model_.ResolvePath(path);
      });
}

void PointerScannerPresenter::OnTargetAddressChanged(uint64_t address) {
  pointer_scanner_model_.SetTargetAddress(address);
}

void PointerScannerPresenter::OnTargetTypeChanged(ScanValueType type) {
  pointer_scanner_model_.SetTargetType(type);
}

void PointerScannerPresenter::OnTargetFromCheatSelected(size_t index) {
  UpdateTargetFromCheatTable(index);
}

void PointerScannerPresenter::OnTargetFromScanSelected(size_t index) {
  UpdateTargetFromScanResults(index);
}

void PointerScannerPresenter::OnGenerateMapPressed() {
  pointer_scanner_model_.GeneratePointerMap();
}

void PointerScannerPresenter::OnFindPathsPressed() {
  auto config = pointer_scanner_view_.GetScanConfig();
  pointer_scanner_model_.FindPaths(config);
}

void PointerScannerPresenter::OnValidatePressed() {
  pointer_scanner_model_.ValidatePathsAsync();
  LogInfo("Validation started");
}

void PointerScannerPresenter::OnValidationComplete(
    const std::vector<core::PointerPath>& valid_paths) {
  pointer_scanner_model_.SetPaths(valid_paths);
  LogInfo("Validation complete: {} paths remain valid", valid_paths.size());
}

void PointerScannerPresenter::OnCancelPressed() {
  pointer_scanner_model_.CancelOperation();
}

void PointerScannerPresenter::OnResultDoubleClicked(size_t index) {
  AddPathToCheatTable(index);
}

void PointerScannerPresenter::OnProgressUpdated(
    float /*progress*/, const std::string& /*operation*/) {
  // Progress is queried by the view during Render(), no action needed here
}

void PointerScannerPresenter::OnPathsUpdated() {
  // Paths were updated (cleared, validated, etc.)
  // The view will pick up the new paths during the next Render() call
}

void PointerScannerPresenter::OnActiveProcessChanged(IProcess* process) {
  if (pointer_scanner_model_.IsBusy()) {
    // Queue the process switch for later
    pending_process_switch_ = process;
    LogInfo("Process change queued - waiting for operation to complete");
  } else {
    // Switch immediately
    pending_process_switch_ = process;
    HandlePendingProcessSwitch();
  }
}

void PointerScannerPresenter::HandlePendingProcessSwitch() {
  if (!pending_process_switch_) {
    return;
  }

  pointer_scanner_model_.SetActiveProcess(pending_process_switch_);
  pending_process_switch_ = nullptr;
  LogInfo("Process switch completed");
}

void PointerScannerPresenter::UpdateTargetFromCheatTable(size_t index) {
  auto entries_ptr = cheat_table_model_.entries();
  const auto& entries = *entries_ptr;

  if (index >= entries.size()) {
    LogWarning("Invalid cheat table index: {}", index);
    return;
  }

  uint64_t address = entries[index].address;
  pointer_scanner_model_.SetTargetAddress(address);
  pointer_scanner_model_.SetTargetType(entries[index].type);
  pointer_scanner_view_.SetSelectedType(entries[index].type);

  LogInfo("Target address set from cheat table: 0x{:X} ({})",
          address,
          entries[index].description);
}

void PointerScannerPresenter::UpdateTargetFromScanResults(size_t index) {
  const auto& results = scan_result_model_.entries();

  if (index >= results.addresses.size()) {
    LogWarning("Invalid scan result index: {}", index);
    return;
  }

  uint64_t address = results.addresses[index];
  pointer_scanner_model_.SetTargetAddress(address);
  pointer_scanner_model_.SetTargetType(results.value_type);
  pointer_scanner_view_.SetSelectedType(results.value_type);

  LogInfo("Target address set from scan results: 0x{:X}", address);
}

void PointerScannerPresenter::AddPathToCheatTable(size_t index) {
  auto paths = pointer_scanner_model_.GetPaths();

  if (index >= paths.size()) {
    LogWarning("Invalid path index: {}", index);
    return;
  }

  const auto& path = paths[index];
  uint64_t target = pointer_scanner_model_.GetTargetAddress();
  ScanValueType type = pointer_scanner_model_.GetTargetType();
  size_t size = GetSizeForType(type);

  // Create description showing the full pointer path with all offsets
  // Format: "module+offset -> off1 -> off2 -> ..."
  std::string description =
      path.module_name.empty()
          ? fmt::format("0x{:X}", path.base_address)
          : fmt::format(
                "\"{}\" + 0x{:X}", path.module_name, path.module_offset);

  for (const auto& offset : path.offsets) {
    if (offset >= 0) {
      description += fmt::format(" -> {:X}", offset);
    } else {
      description += fmt::format(" -> -{:X}", -offset);
    }
  }

  // Add as pointer chain entry - this will dynamically resolve to the target
  cheat_table_model_.AddPointerChainEntry(path.base_address,
                                          path.offsets,
                                          path.module_name,
                                          path.module_offset,
                                          type,
                                          description,
                                          size);

  LogInfo("Added pointer chain to cheat table: {}", description);
}

}  // namespace maia
