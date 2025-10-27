// Copyright (c) Maia

#pragma once

#include <cstdint>
#include <string>

// TODO: make a global dispatcher through a simpler interface. By doing this,
// the dispatcher parameter will be removed from the ShowProcessTool. Study if
// this is necessary.
#include <entt/signal/dispatcher.hpp>

namespace maia::gui {

struct EventPickedProcess {
  uint32_t pid;
  std::string name;
};

// TODO: Adjust the return of this function.
void ShowProcessTool(entt::dispatcher& dispatcher, bool* p_open = nullptr);

}  // namespace maia::gui
