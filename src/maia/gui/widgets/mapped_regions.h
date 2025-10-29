// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>

#include "maia/logging.h"

namespace maia::gui {

class MappedRegionsWidget {
 public:
  MappedRegionsWidget() {
    scan_mem_regions_sink_.connect<&MappedRegionsWidget::OnButtonPressed>(this);
  }

  void Render() {
    if (ImGui::Begin("Mapped regions")) {
      if (ImGui::Button("ButtonPress")) {
        scan_regions_button_.publish();
      }
    }
    ImGui::End();
  }

 private:
  void OnButtonPressed() {
    LogInfo("ButtonPressed.");
  }

  // Signals:
  entt::sigh<void()> scan_regions_button_;

  // Slots:
  entt::sink<entt::sigh<void()>> scan_mem_regions_sink_{scan_regions_button_};
};

}  // namespace maia::gui
