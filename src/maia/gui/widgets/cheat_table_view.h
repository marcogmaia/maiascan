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
  struct AddDialogState {
    bool show = false;
    std::string address_input;
    std::string description_input;
    int type_index = 4;  // Default to Int32
    float pos_x = 0.0f;
    float pos_y = 0.0f;
  };

  struct AddDialogActions {
    std::function<void(std::string, ScanValueType, std::string)> on_add;
    std::function<void()> on_close;
  };

  void RenderAddDialog(const AddDialogActions& actions);

  class Signals {
   public:
    /// \brief Emitted when the user toggles the freeze status for an entry.
    /// \param index The index of the entry in the table.
    entt::sigh<void(size_t index)> freeze_toggled;

    /// \brief Emitted when the user changes the description of an entry.
    /// \param index The index of the entry in the table.
    /// \param new_desc The new description provided by the user.
    entt::sigh<void(size_t index, std::string new_desc)> description_changed;

    /// \brief Emitted when the user toggles hex display for an entry.
    /// \param index The index of the entry in the table.
    /// \param show_as_hex True if hex should be shown.
    entt::sigh<void(size_t index, bool show_as_hex)> hex_display_toggled;

    /// \brief Emitted when the user attempts to set a new value for an entry.
    /// \param index The index of the entry in the table.
    /// \param new_val The string representation of the new value.
    entt::sigh<void(size_t index, std::string new_val)> value_changed;

    /// \brief Emitted when the user requests a type change for an entry.
    /// \param index The index of the entry in the table.
    /// \param new_type The new value type.
    entt::sigh<void(size_t index, ScanValueType new_type)>
        type_change_requested;

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
    auto HexDisplayToggled() { return entt::sink(view.signals_.hex_display_toggled); }
    auto ValueChanged() { return entt::sink(view.signals_.value_changed); }
    auto TypeChangeRequested() { return entt::sink(view.signals_.type_change_requested); }
    auto DeleteRequested() { return entt::sink(view.signals_.delete_requested); }
    auto SaveRequested() { return entt::sink(view.signals_.save_requested); }
    auto LoadRequested() { return entt::sink(view.signals_.load_requested); }
    auto AddManualRequested() { return entt::sink(view.signals_.add_manual_requested); }

    // clang-format on
  };

  Signals signals_;
  int selected_row_index_ = -1;
  AddDialogState add_dialog_;
};

}  // namespace maia
