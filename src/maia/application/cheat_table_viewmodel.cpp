// Copyright (c) Maia

#include "maia/application/cheat_table_viewmodel.h"

#include <filesystem>

#include "maia/application/file_dialogs.h"
#include "maia/core/address_parser.h"
#include "maia/logging.h"

namespace maia {

CheatTableViewModel::CheatTableViewModel(CheatTableModel& model,
                                         ProcessModel& process_model,
                                         gui::CheatTableState& state)
    : model_(model),
      process_model_(process_model),
      state_(state) {}

void CheatTableViewModel::OnFreezeToggled(size_t index) {
  model_.ToggleFreeze(index);
}

void CheatTableViewModel::OnDescriptionChanged(size_t index,
                                               std::string new_desc) {
  model_.UpdateEntryDescription(index, new_desc);
}

void CheatTableViewModel::OnHexDisplayToggled(size_t index, bool show_as_hex) {
  model_.SetShowAsHex(index, show_as_hex);
}

void CheatTableViewModel::OnValueChanged(size_t index, std::string new_val) {
  model_.SetValue(index, new_val);
}

void CheatTableViewModel::OnTypeChangeRequested(size_t index,
                                                ScanValueType new_type) {
  model_.ChangeEntryType(index, new_type);
}

void CheatTableViewModel::OnDeleteRequested(size_t index) {
  model_.RemoveEntry(index);
}

void CheatTableViewModel::OnSaveRequested() {
  using application::FileDialogs;
  using application::FileFilter;

  constexpr FileFilter kFilters[] = {
      {.name = "JSON Files", .spec = "json"},
      { .name = "All Files",    .spec = "*"}
  };

  std::optional<std::filesystem::path> default_path;
  if (!last_save_path_.empty()) {
    default_path = std::filesystem::path(last_save_path_);
  }

  auto result =
      FileDialogs::ShowSaveDialog(kFilters, default_path, "cheat_table.json");
  if (!result) {
    return;
  }

  auto save_path = *result;
  if (save_path.extension() != ".json") {
    save_path += ".json";
  }

  if (model_.Save(save_path)) {
    last_save_path_ = save_path.string();
    LogInfo("Cheat table saved to {}", save_path.string());
  } else {
    LogError("Failed to save cheat table");
  }
}

void CheatTableViewModel::OnLoadRequested() {
  using application::FileDialogs;
  using application::FileFilter;

  constexpr FileFilter kFilters[] = {
      {.name = "JSON Files", .spec = "json"},
      { .name = "All Files",    .spec = "*"}
  };

  std::optional<std::filesystem::path> default_path;
  if (!last_save_path_.empty()) {
    default_path = std::filesystem::path(last_save_path_);
  }

  auto result = FileDialogs::ShowOpenDialog(kFilters, default_path);
  if (!result) {
    return;
  }

  const auto& load_path = *result;
  if (model_.Load(load_path)) {
    last_save_path_ = load_path.string();
    LogInfo("Cheat table loaded from {}", load_path.string());
  } else {
    LogError("Failed to load cheat table");
  }
}

void CheatTableViewModel::OnAddManualRequested(std::string address_str,
                                               ScanValueType type,
                                               std::string description) {
  auto parsed =
      ParseAddressExpression(address_str, process_model_.GetActiveProcess());
  if (!parsed) {
    LogWarning("Failed to parse address: {}", address_str);
    return;
  }

  model_.AddEntry(parsed->resolved_address, type, description);
}

}  // namespace maia
