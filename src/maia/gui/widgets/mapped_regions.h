// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>

#include "maia/logging.h"

namespace maia::gui {

//
class MappedRegionsWidget {
 public:
  MappedRegionsWidget() {
    scan_mem_regions_sink_.connect<[]() { LogInfo("ButtonPressed."); }>();
  }

  // Render widget.
  void operator()() {
    if (ImGui::Button("ButtonPress")) {
      scan_mem_regions_button_.publish();
    }
  }

 private:
  void OnButtonPressed() {
    LogInfo("ButtonPressed.");
  }

  entt::sigh<void()> scan_mem_regions_button_;
  entt::sink<entt::sigh<void()>> scan_mem_regions_sink_;
};

}  // namespace maia::gui
