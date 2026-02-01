// Copyright (c) Maia

#include "maia/gui/widgets/cheat_table_view.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>

#include "maia/core/scan_types.h"
#include "maia/core/value_formatter.h"

namespace maia {

namespace {

// Helper to lerp between two ImVec4 colors
// t=0 returns start_color, t=1 returns end_color
ImVec4 LerpColor(const ImVec4& start_color, const ImVec4& end_color, float t) {
  return ImVec4(std::lerp(start_color.x, end_color.x, t),
                std::lerp(start_color.y, end_color.y, t),
                std::lerp(start_color.z, end_color.z, t),
                std::lerp(start_color.w, end_color.w, t));
}

}  // namespace

void CheatTableView::Render(const std::vector<CheatTableEntry>& entries) {
  if (ImGui::Begin("Cheat Table")) {
    // Render toolbar
    RenderToolbar();
    ImGui::Separator();

    static ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("CheatTable", 5, flags)) {
      ImGui::TableSetupColumn(
          "Active", ImGuiTableColumnFlags_WidthFixed, 50.0f);
      ImGui::TableSetupColumn("Description");
      ImGui::TableSetupColumn("Address");
      ImGui::TableSetupColumn("Type");
      ImGui::TableSetupColumn("Value");
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];

        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();

        // Snapshot of entry data for thread-safe UI rendering
        std::string val_str;
        bool is_frozen = false;
        std::chrono::steady_clock::time_point last_change_time;

        {
          val_str =
              ValueFormatter::Format(entry.data->GetValue(), entry.type, false);
          is_frozen = entry.data->IsFrozen();
          last_change_time = entry.data->GetLastChangeTime();
        }

        // Calculate blink effect based on time since last change
        // Fade from red to white over 1 second
        constexpr float kBlinkDuration = 1.0f;  // seconds
        auto now = std::chrono::steady_clock::now();
        float time_since_change =
            std::chrono::duration<float>(now - last_change_time).count();
        float blink_alpha = 0.0f;
        if (time_since_change < kBlinkDuration &&
            last_change_time.time_since_epoch().count() > 0) {
          blink_alpha = 1.0f - (time_since_change / kBlinkDuration);
        }

        // 1. Frozen Checkbox
        ImGui::TableSetColumnIndex(0);
        bool frozen = is_frozen;
        if (ImGui::Checkbox("##frozen", &frozen)) {
          signals_.freeze_toggled.publish(i);
        }

        // 2. Description (Editable)
        ImGui::TableSetColumnIndex(1);
        std::string desc_buffer = entry.description;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText(
                "##desc", &desc_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
          signals_.description_changed.publish(i, desc_buffer);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
          signals_.description_changed.publish(i, desc_buffer);
        }

        // 3. Address (Read-only)
        ImGui::TableSetColumnIndex(2);
        if (entry.IsPointerChain()) {
          // Show pointer chain info
          if (!entry.pointer_module.empty()) {
            ImGui::Text("[%s+%llX]",
                        entry.pointer_module.c_str(),
                        entry.pointer_module_offset);
          } else {
            ImGui::Text("[0x%llX]", entry.pointer_base);
          }
        } else {
          ImGui::Text("0x%llX", entry.address);
        }

        // 4. Type (Read-only)
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%s", ValueFormatter::GetLabel(entry.type));

        // 5. Value (Editable with blink effect on change)
        ImGui::TableSetColumnIndex(4);
        ImGui::SetNextItemWidth(-FLT_MIN);

        // Apply red->white fade if value recently changed
        if (blink_alpha > 0.0f) {
          ImVec4 default_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
          const auto color_red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
          ImVec4 blink_color = LerpColor(default_color, color_red, blink_alpha);
          ImGui::PushStyleColor(ImGuiCol_Text, blink_color);
        }

        if (ImGui::InputText(
                "##value", &val_str, ImGuiInputTextFlags_EnterReturnsTrue)) {
          signals_.value_changed.publish(i, val_str);
        }

        if (blink_alpha > 0.0f) {
          ImGui::PopStyleColor();
        }

        // Context Menu for Delete
        if (ImGui::BeginPopupContextItem("row_context")) {
          if (ImGui::Selectable("Delete")) {
            signals_.delete_requested.publish(i);
          }
          ImGui::EndPopup();
        }

        ImGui::PopID();
      }

      ImGui::EndTable();
    }

    // Render add dialog if open
    RenderAddDialog();
  }
  ImGui::End();
}

void CheatTableView::RenderToolbar() {
  if (ImGui::Button("Save")) {
    signals_.save_requested.publish();
  }
  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    signals_.load_requested.publish();
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Address")) {
    show_add_dialog_ = true;
    add_address_input_.clear();
    add_description_input_.clear();
    add_type_index_ = 4;  // Default to Int32
  }
}

void CheatTableView::RenderAddDialog() {
  if (!show_add_dialog_) {
    return;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Add Address",
                             &show_add_dialog_,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Address (e.g., 0x1234 or game.exe+0x100):");
    ImGui::InputText("##addr", &add_address_input_);

    ImGui::Text("Type:");
    if (ImGui::BeginCombo(
            "##type",
            ValueFormatter::GetLabel(kAllScanValueTypes[add_type_index_]))) {
      for (size_t i = 0; i < kAllScanValueTypes.size(); i++) {
        const bool is_selected = (add_type_index_ == i);
        if (ImGui::Selectable(ValueFormatter::GetLabel(kAllScanValueTypes[i]),
                              is_selected)) {
          add_type_index_ = i;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::Text("Description:");
    ImGui::InputText("##desc", &add_description_input_);

    ImGui::Separator();

    if (ImGui::Button("Add", ImVec2(120, 0))) {
      if (!add_address_input_.empty()) {
        signals_.add_manual_requested.publish(
            add_address_input_,
            kAllScanValueTypes[add_type_index_],
            add_description_input_);
        show_add_dialog_ = false;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      show_add_dialog_ = false;
    }

    ImGui::EndPopup();
  }

  // Open the popup if dialog should be shown
  if (show_add_dialog_ && !ImGui::IsPopupOpen("Add Address")) {
    ImGui::OpenPopup("Add Address");
  }
}

}  // namespace maia
