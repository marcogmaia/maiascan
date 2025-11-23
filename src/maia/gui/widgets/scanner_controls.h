// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>
#include "maia/core/scan_types.h"

namespace maia {

class ScannerControls {
 public:
  struct Signals {
    entt::sigh<void()> new_scan_pressed;
    entt::sigh<void()> next_scan_pressed;
    entt::sigh<void(ScanComparison)> comparison_selected;
  };

  void Render(bool is_scanning) const {
    if (is_scanning) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("New Scan")) {
      signals_.new_scan_pressed.publish();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Scan")) {
      signals_.next_scan_pressed.publish();
    }

    // Comparison Dropdown logic...

    if (is_scanning) {
      ImGui::EndDisabled();
    }
  }

  // Expose sinks pattern
  auto Sinks() {
    return SinksWrapper{*this};
  }

 private:
  struct SinksWrapper {
    ScannerControls& parent;

    // clang-format off
    auto NewScanPressed() { return entt::sink(parent.signals_.new_scan_pressed); }

    // clang-format on

    // ... others
  };

  Signals signals_;
  // Presentation State specific to inputs
  int selected_comparison_idx_ = 0;
};

}  // namespace maia
