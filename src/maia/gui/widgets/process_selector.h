// Copyright (c) Maia

#pragma once

#include <array>
#include <string>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "maia/core/memory_common.h"

namespace maia::gui {

// ProcessSelector (View).
//
// Renders the process selection window. It relies on a Presenter to provide it
// with the process list and current attached process state. It emits signals
// when the user interacts with it.
class ProcessSelector {
 public:
  // A Presenter will connect its slots to these signals.
  struct Signals {
    // Fired when the user clicks the "Refresh" button
    entt::sigh<void()> refresh_requested;

    // Fired when the "Pick (Drag Me)" button is released over a window
    entt::sigh<void(Pid pid, const std::string& name)> process_selected;
  };

  // Main render function.
  // A Presenter should call this every frame.
  void Render(bool* p_open,
              const std::vector<ProcessInfo>& processes,
              const std::string& attached_process_name,
              Pid attached_pid);

  Signals& GetSignals() {
    return signals_;
  }

 private:
  // Renders the "Pick (Drag Me)" button.
  void RenderProcessPickerButton() const;

  std::array<char, 260> filter_{};
  Signals signals_;
};

}  // namespace maia::gui
