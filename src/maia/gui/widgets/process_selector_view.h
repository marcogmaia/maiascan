// Copyright (c) Maia

#pragma once

#include <array>

#include <entt/signal/sigh.hpp>

#include "maia/core/memory_common.h"
#include "maia/gui/models/ui_state.h"

namespace maia {

// ProcessSelector (View).
//
// Renders the process selection window. It relies on a Presenter to provide it
// with the process list and current attached process state. It emits signals
// when the user interacts with it.
class ProcessSelectorView {
 public:
  struct Signals {
    // Fired when the user clicks the "Refresh" button.
    entt::sigh<void()> refresh_requested;

    // Fired when selecting a process from the list.
    entt::sigh<void(Pid pid)> process_selected_from_list;

    // Fired when the "Pick (Drag Me)" button is released.
    entt::sigh<void()> process_pick_requested;
  };

  struct Sinks {
    ProcessSelectorView& view;

    // clang-format off
    auto RefreshRequested() { return entt::sink(view.signals_.refresh_requested); };
    auto ProcessSelectedFromList() { return entt::sink(view.signals_.process_selected_from_list); };
    auto ProcessPickRequested() { return entt::sink(view.signals_.process_pick_requested); };

    // clang-format on
  };

  Sinks sinks() {
    return Sinks{*this};
  }

  // Main render function. Renders the window if visible.
  void Render(gui::ProcessSelectorState& state);

  Signals& signals() {
    return signals_;
  }

 private:
  // Renders the "Pick (Drag Me)" button.
  void RenderProcessPickerButton() const;

  std::array<char, 260> filter_{};
  Signals signals_;
};

// Renders a compact toolbar for process selection.
// Returns true if the "Select..." button was clicked.
[[nodiscard]] bool RenderToolbar(const gui::ProcessSelectorState& state);

}  // namespace maia
