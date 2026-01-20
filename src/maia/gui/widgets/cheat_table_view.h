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
    entt::sigh<void(size_t index)> freeze_toggled;
    entt::sigh<void(size_t index, std::string new_desc)> description_changed;
    entt::sigh<void(size_t index, std::string new_val)> value_changed;
    entt::sigh<void(size_t index)> delete_requested;
  };

  struct Sinks {
    CheatTableView& view;

    auto FreezeToggled() {
      return entt::sink(view.signals_.freeze_toggled);
    }

    auto DescriptionChanged() {
      return entt::sink(view.signals_.description_changed);
    }

    auto ValueChanged() {
      return entt::sink(view.signals_.value_changed);
    }

    auto DeleteRequested() {
      return entt::sink(view.signals_.delete_requested);
    }
  };

  Signals signals_;
  int selected_row_index_ = -1;
};

}  // namespace maia
