// Copyright (c) Maia

#include "maia/application/cheat_table_presenter.h"

#include <filesystem>
#include <string>

#include "maia/application/file_dialogs.h"
#include "maia/core/address_parser.h"
#include "maia/core/signal_utils.h"
#include "maia/logging.h"

namespace maia {

CheatTablePresenter::CheatTablePresenter(CheatTableModel& model,
                                         CheatTableView& view)
    : model_(model),
      view_(view) {
  // clang-format off
  // Model signals -> Presenter handlers
  Connect(connections_, model_.sinks().TableChanged(), this, Slot<&CheatTablePresenter::OnTableChanged>);
  
  // View signals -> Presenter handlers
  Connect(connections_, view_.sinks().FreezeToggled(),      this, Slot<&CheatTablePresenter::OnFreezeToggled>);
  Connect(connections_, view_.sinks().DescriptionChanged(), this, Slot<&CheatTablePresenter::OnDescriptionChanged>);
  Connect(connections_, view_.sinks().HexDisplayToggled(),  this, Slot<&CheatTablePresenter::OnHexDisplayToggled>);
  Connect(connections_, view_.sinks().TypeChangeRequested(), this, Slot<&CheatTablePresenter::OnTypeChangeRequested>);
  Connect(connections_, view_.sinks().ValueChanged(),       this, Slot<&CheatTablePresenter::OnValueChanged>);
  Connect(connections_, view_.sinks().DeleteRequested(),    this, Slot<&CheatTablePresenter::OnDeleteRequested>);
  Connect(connections_, view_.sinks().SaveRequested(),      this, Slot<&CheatTablePresenter::OnSaveRequested>);
  Connect(connections_, view_.sinks().LoadRequested(),      this, Slot<&CheatTablePresenter::OnLoadRequested>);
  Connect(connections_, view_.sinks().AddManualRequested(), this, Slot<&CheatTablePresenter::OnAddManualRequested>);
  // clang-format on
}

void CheatTablePresenter::Render() {
  auto snapshot = model_.entries();
  view_.Render(*snapshot);
}

void CheatTablePresenter::OnTableChanged() {
  // Table changed notification - UI will pick up changes on next Render
}

void CheatTablePresenter::OnFreezeToggled(size_t index) {
  model_.ToggleFreeze(index);
}

void CheatTablePresenter::OnDescriptionChanged(size_t index,
                                               std::string new_desc) {
  model_.UpdateEntryDescription(index, new_desc);
}

void CheatTablePresenter::OnHexDisplayToggled(size_t index, bool show_as_hex) {
  model_.SetShowAsHex(index, show_as_hex);
}

void CheatTablePresenter::OnTypeChangeRequested(size_t index,
                                                ScanValueType new_type) {
  model_.ChangeEntryType(index, new_type);
}

void CheatTablePresenter::OnValueChanged(size_t index, std::string new_val) {
  model_.SetValue(index, new_val);
}

void CheatTablePresenter::OnDeleteRequested(size_t index) {
  model_.RemoveEntry(index);
}

void CheatTablePresenter::OnSaveRequested() {
  using application::FileDialogs;
  using application::FileFilter;

  constexpr FileFilter kFilters[] = {
      {"JSON Files", "json"},
      { "All Files",    "*"}
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

void CheatTablePresenter::OnLoadRequested() {
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

  auto load_path = *result;
  if (model_.Load(load_path)) {
    last_save_path_ = load_path.string();
    LogInfo("Cheat table loaded from {}", load_path.string());
  } else {
    LogError("Failed to load cheat table");
  }
}

void CheatTablePresenter::OnAddManualRequested(std::string address_str,
                                               ScanValueType type,
                                               std::string description) {
  // Parse the address expression
  // For now, we need access to the active process to resolve module names
  // TODO(marco): Get the active process from somewhere (ProcessModel?)
  // For now, just parse as a simple number

  auto parsed = ParseAddressExpression(address_str, nullptr);
  if (!parsed) {
    LogWarning("Failed to parse address: {}", address_str);
    return;
  }

  if (!parsed->module_name.empty()) {
    // This is a module-relative address
    // Store it with module info
    model_.AddEntry(parsed->resolved_address, type, description);
    LogInfo("Added manual entry: {} at {}+{}",
            description,
            parsed->module_name,
            parsed->module_offset);
  } else {
    // This is a static address
    model_.AddEntry(parsed->resolved_address, type, description);
    LogInfo("Added manual entry: {} at 0x{:X}",
            description,
            parsed->resolved_address);
  }
}

}  // namespace maia
