// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>
#include <string>
#include <vector>

#include "maia/application/cheat_table_model.h"

namespace maia {

class CheatTableView {
 public:
  void Render(const std::vector<CheatTableEntry>& entries);

  auto sinks() {
    return Sinks{*this};
  }

 private:
  void RenderToolbar();
  void RenderAddDialog();

  class Signals {
   public:
    /// \brief Emitted when the user toggles the freeze status for an entry.
    /// \param index The index of the entry in the table.
    entt::sigh<void(size_t index)> freeze_toggled;

    /// \brief Emitted when the user changes the description of an entry.
    /// \param index The index of the entry in the table.
    /// \param new_desc The new description provided by the user.
    entt::sigh<void(size_t index, std::string new_desc)> description_changed;

    /// \brief Emitted when the user attempts to set a new value for an entry.
    /// \param index The index of the entry in the table.
    /// \param new_val The string representation of the new value.
    entt::sigh<void(size_t index, std::string new_val)> value_changed;

    /// \brief Emitted when the user requests the deletion of an entry.
    /// \param index The index of the entry in the table.
    entt::sigh<void(size_t index)> delete_requested;

    /// \brief Emitted when the user requests to save the table.
    entt::sigh<void()> save_requested;

    /// \brief Emitted when the user requests to load the table.
    entt::sigh<void()> load_requested;

    /// \brief Emitted when the user adds a manual entry.
    /// \param address The parsed address expression.
    /// \param type The value type.
    /// \param description The entry description.
    entt::sigh<void(
        std::string address, ScanValueType type, std::string description)>
        add_manual_requested;
  };

  struct Sinks {
    CheatTableView& view;

    // clang-format off
    auto FreezeToggled() { return entt::sink(view.signals_.freeze_toggled); }
    auto DescriptionChanged() { return entt::sink(view.signals_.description_changed); }
    auto ValueChanged() { return entt::sink(view.signals_.value_changed); }
    auto DeleteRequested() { return entt::sink(view.signals_.delete_requested); }
    auto SaveRequested() { return entt::sink(view.signals_.save_requested); }
    auto LoadRequested() { return entt::sink(view.signals_.load_requested); }
    auto AddManualRequested() { return entt::sink(view.signals_.add_manual_requested); }

    // clang-format on
  };

  Signals signals_;
  int selected_row_index_ = -1;

  // Add dialog state
  bool show_add_dialog_ = false;
  std::string add_address_input_;
  std::string add_description_input_;
  int add_type_index_ = 4;  // Default to Int32
};

}  // namespace maia
