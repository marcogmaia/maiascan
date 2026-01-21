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
  };

  struct Sinks {
    CheatTableView& view;

    // clang-format off
    auto FreezeToggled() { return entt::sink(view.signals_.freeze_toggled); }
    auto DescriptionChanged() { return entt::sink(view.signals_.description_changed); }
    auto ValueChanged() { return entt::sink(view.signals_.value_changed); }
    auto DeleteRequested() { return entt::sink(view.signals_.delete_requested); }

    // clang-format on
  };

  Signals signals_;
  int selected_row_index_ = -1;
};

}  // namespace maia
