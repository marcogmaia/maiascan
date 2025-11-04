// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"
#include "maia/logging.h"

namespace maia {

class ScannerWidget {
 public:
  class Signals {
   public:
    entt::sigh<void()> scan_button_pressed;
  };

  void Render(const std::vector<ScanEntry>& entries) const {
    if (ImGui::Begin("Mapped regions")) {
      if (ImGui::Button("Scan")) {
        signals_.scan_button_pressed.publish();
      }

      if (ImGui::BeginTable("Tab", 2)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Value");

        ImGui::TableHeadersRow();
        for (const auto& entry : entries) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(std::format("{:x}", entry.address).c_str());
          ImGui::TableNextColumn();
          auto val = *reinterpret_cast<const uint32_t*>(entry.data.data());
          ImGui::TextUnformatted(std::format("{}", val).c_str());
        }
        ImGui::EndTable();
      }
    }
    ImGui::End();
  }

  Signals& signals() {
    return signals_;
  }

 private:
  Signals signals_;
};

}  // namespace maia
