// Copyright (c) Maia

#include "maia/application/pointer_scanner_viewmodel.h"

#include <filesystem>

#include "maia/application/file_dialogs.h"
#include "maia/core/signal_utils.h"
#include "maia/logging.h"

namespace maia {

PointerScannerViewModel::PointerScannerViewModel(
    PointerScannerModel& pointer_scanner_model,
    ProcessModel& process_model,
    CheatTableModel& cheat_table_model,
    ScanResultModel& scan_result_model,
    gui::PointerScannerState& state)
    : pointer_scanner_model_(pointer_scanner_model),
      process_model_(process_model),
      cheat_table_model_(cheat_table_model),
      scan_result_model_(scan_result_model),
      state_(state) {
  // Connect Model signals
  // clang-format off
  Connect(connections_, pointer_scanner_model_.sinks().ValidationComplete(), this, Slot<&PointerScannerViewModel::OnValidationComplete>);
  Connect(connections_, process_model_.sinks().ActiveProcessChanged(),       this, Slot<&PointerScannerViewModel::OnActiveProcessChanged>);
  // clang-format on
}

void PointerScannerViewModel::Update() {
  pointer_scanner_model_.Update();
  state_.is_generating_map = pointer_scanner_model_.IsGeneratingMap();
  state_.is_scanning = pointer_scanner_model_.IsScanning();
  state_.map_entry_count = pointer_scanner_model_.GetMapEntryCount();
  state_.map_progress = pointer_scanner_model_.GetMapProgress();
  state_.scan_progress = pointer_scanner_model_.GetScanProgress();
}

void PointerScannerViewModel::OnTargetAddressChanged(uint64_t address) {
  pointer_scanner_model_.SetTargetAddress(address);
}

void PointerScannerViewModel::OnTargetTypeChanged(ScanValueType type) {
  pointer_scanner_model_.SetTargetType(type);
  state_.value_type = type;
}

void PointerScannerViewModel::OnTargetFromCheatSelected(size_t index) {
  auto snapshot = cheat_table_model_.entries();
  if (index < snapshot->size()) {
    const auto& entry = (*snapshot)[index];
    pointer_scanner_model_.SetTargetAddress(entry.address);
    pointer_scanner_model_.SetTargetType(entry.type);
    state_.value_type = entry.type;
  }
}

void PointerScannerViewModel::OnTargetFromScanSelected(size_t index) {
  const auto& results = scan_result_model_.entries();
  if (index < results.addresses.size()) {
    pointer_scanner_model_.SetTargetAddress(results.addresses[index]);
    pointer_scanner_model_.SetTargetType(
        scan_result_model_.GetSessionConfig().value_type);
    state_.value_type = scan_result_model_.GetSessionConfig().value_type;
  }
}

void PointerScannerViewModel::OnGenerateMapPressed() {
  pointer_scanner_model_.GeneratePointerMap();
}

void PointerScannerViewModel::OnSaveMapPressed() {
  using application::FileDialogs;
  using application::FileFilter;
  constexpr FileFilter kFilters[] = {
      {.name = "Pointer Map", .spec = "pmap"}
  };
  auto result =
      FileDialogs::ShowSaveDialog(kFilters, std::nullopt, "process.pmap");
  if (result) {
    if (!pointer_scanner_model_.SaveMap(*result)) {
      LogWarning("Failed to save pointer map to {}", result->string());
    }
  }
}

void PointerScannerViewModel::OnLoadMapPressed() {
  using application::FileDialogs;
  using application::FileFilter;
  constexpr FileFilter kFilters[] = {
      {.name = "Pointer Map", .spec = "pmap"}
  };
  auto result = FileDialogs::ShowOpenDialog(kFilters, std::nullopt);
  if (result) {
    if (!pointer_scanner_model_.LoadMap(*result)) {
      LogWarning("Failed to load pointer map from {}", result->string());
    }
  }
}

void PointerScannerViewModel::OnFindPathsPressed(
    const core::PointerScanConfig& config) {
  pointer_scanner_model_.FindPaths(config);
}

void PointerScannerViewModel::OnValidatePressed() {
  pointer_scanner_model_.ValidatePathsAsync();
}

void PointerScannerViewModel::OnCancelPressed() {
  pointer_scanner_model_.CancelOperation();
}

void PointerScannerViewModel::OnResultDoubleClicked(size_t index) {
  auto paths = pointer_scanner_model_.GetPaths();
  if (index < paths.size()) {
    const auto& path = paths[index];
    auto resolved = pointer_scanner_model_.ResolvePath(path);
    if (resolved) {
      // Add as pointer chain entry to preserve the full path information
      // for dynamic resolution on subsequent process launches.
      cheat_table_model_.AddPointerChainEntry(
          path.base_address,
          path.offsets,
          path.module_name,
          path.module_offset,
          pointer_scanner_model_.GetTargetType(),
          "Pointer Path Result");
    }
  }
}

void PointerScannerViewModel::OnShowAllPressed() {
  state_.show_all_results = !state_.show_all_results;
}

std::optional<std::vector<std::byte>> PointerScannerViewModel::GetValue(
    uintptr_t address) {
  IProcess* process = process_model_.GetActiveProcess();
  if (!process) {
    return std::nullopt;
  }

  size_t size = GetSizeForType(state_.value_type);
  return value_cache_.Get(address, [process, size](uint64_t addr) {
    std::vector<std::byte> buffer(size);
    MemoryAddress addr_arr[] = {addr};
    if (process->ReadMemory({addr_arr, 1}, size, buffer, nullptr)) {
      return std::make_optional(std::move(buffer));
    }
    return std::optional<std::vector<std::byte>>{std::nullopt};
  });
}

void PointerScannerViewModel::OnValidationComplete(
    const std::vector<core::PointerPath>& valid_paths) {
  pointer_scanner_model_.SetPaths(valid_paths);
}

void PointerScannerViewModel::OnActiveProcessChanged(IProcess* process) {
  pointer_scanner_model_.SetActiveProcess(process);
  value_cache_.Clear();
}

}  // namespace maia
