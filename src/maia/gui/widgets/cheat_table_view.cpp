// Copyright (c) Maia

#include "maia/gui/widgets/cheat_table_view.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <span>
#include <vector>

#include <fmt/format.h>

#include "maia/core/scan_types.h"
#include "maia/core/value_formatter.h"

namespace maia {

namespace {

// --- Callbacks for Decoupling ---

struct ToolbarActions {
  std::function<void()> on_save;
  std::function<void()> on_load;
  std::function<void(ImVec2)> on_add_clicked;
};

struct TableActions {
  std::function<void(size_t)> on_freeze_toggled;
  std::function<void(size_t, std::string)> on_description_changed;
  std::function<void(size_t, bool)> on_hex_display_toggled;
  std::function<void(size_t, ScanValueType)> on_type_change_requested;
  std::function<void(size_t, std::string)> on_value_changed;
  std::function<void(size_t)> on_delete_requested;
};

// --- Helper Functions ---

ImVec4 LerpColor(const ImVec4& start_color, const ImVec4& end_color, float t) {
  return ImVec4(std::lerp(start_color.x, end_color.x, t),
                std::lerp(start_color.y, end_color.y, t),
                std::lerp(start_color.z, end_color.z, t),
                std::lerp(start_color.w, end_color.w, t));
}

std::string FormatAddress(const CheatTableEntry& entry) {
  const MemoryAddress resolved = entry.data->GetResolvedAddress();
  if (!entry.IsDynamicAddress()) {
    return fmt::format("0x{:X}", resolved != 0 ? resolved : entry.address);
  }

  std::string addr_str;
  if (!entry.pointer_module.empty()) {
    addr_str = fmt::format(
        "[{}+0x{:X}]", entry.pointer_module, entry.pointer_module_offset);
  } else {
    addr_str = fmt::format("[0x{:X}]", entry.pointer_base);
  }

  for (int64_t offset : entry.pointer_offsets) {
    if (offset < 0) {
      addr_str += fmt::format(", -0x{:X}", -offset);
    } else {
      addr_str += fmt::format(", 0x{:X}", offset);
    }
  }

  return addr_str + fmt::format(" -> 0x{:X}", resolved);
}

float CalculateBlinkAlpha(
    std::chrono::steady_clock::time_point last_change_time) {
  constexpr float kBlinkDuration = 1.0f;
  auto now = std::chrono::steady_clock::now();
  float time_since_change =
      std::chrono::duration<float>(now - last_change_time).count();

  if (time_since_change < kBlinkDuration &&
      last_change_time.time_since_epoch().count() > 0) {
    return 1.0f - (time_since_change / kBlinkDuration);
  }
  return 0.0f;
}

// --- Stateless Rendering Functions ---

void RenderToolbar(const ToolbarActions& actions) {
  if (ImGui::Button("Save")) {
    actions.on_save();
  }
  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    actions.on_load();
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Address")) {
    actions.on_add_clicked(
        ImVec2(ImGui::GetItemRectMax().x + 5.0f, ImGui::GetItemRectMin().y));
  }
}

void RenderRowInteractions(const CheatTableEntry& entry) {
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup("row_context");
  }

  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(
        fmt::format("Description: {}", entry.description).c_str());

    const MemoryAddress resolved = entry.data->GetResolvedAddress();
    if (entry.IsDynamicAddress()) {
      if (!entry.pointer_module.empty()) {
        ImGui::TextUnformatted(fmt::format("Base: {} + 0x{:X}",
                                           entry.pointer_module,
                                           entry.pointer_module_offset)
                                   .c_str());
      } else {
        ImGui::TextUnformatted(
            fmt::format("Base Address: 0x{:X}", entry.pointer_base).c_str());
      }
      for (auto offset : entry.pointer_offsets) {
        ImGui::TextUnformatted(
            fmt::format("  -> Offset: 0x{:X}", offset).c_str());
      }
      ImGui::TextUnformatted(
          fmt::format("Resolved Address: 0x{:X}", resolved).c_str());
    } else {
      ImGui::TextUnformatted(fmt::format("Address: 0x{:X}", resolved).c_str());
    }

    ImGui::TextUnformatted(
        fmt::format("Type: {}", ValueFormatter::GetLabel(entry.type)).c_str());
    ImGui::EndTooltip();
  }
}

void RenderRow(const CheatTableEntry& entry,
               size_t index,
               const TableActions& actions) {
  ImGui::PushID(static_cast<int>(index));
  ImGui::TableNextRow();

  // 1. Frozen Checkbox
  ImGui::TableSetColumnIndex(0);
  bool frozen = entry.data->IsFrozen();
  if (ImGui::Checkbox("##frozen", &frozen)) {
    actions.on_freeze_toggled(index);
  }
  RenderRowInteractions(entry);

  // 2. Description (Editable)
  ImGui::TableSetColumnIndex(1);
  std::string desc_buffer = entry.description;
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::InputText(
          "##desc", &desc_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
    actions.on_description_changed(index, desc_buffer);
  }
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    actions.on_description_changed(index, desc_buffer);
  }
  RenderRowInteractions(entry);

  // 3. Address (Read-only)
  ImGui::TableSetColumnIndex(2);
  ImGui::TextUnformatted(FormatAddress(entry).c_str());
  RenderRowInteractions(entry);

  // 4. Type (Read-only)
  ImGui::TableSetColumnIndex(3);
  ImGui::TextUnformatted(ValueFormatter::GetLabel(entry.type));
  RenderRowInteractions(entry);

  // 5. Value (Editable with blink effect on change)
  ImGui::TableSetColumnIndex(4);
  ImGui::SetNextItemWidth(-FLT_MIN);

  std::string val_str = ValueFormatter::Format(
      entry.data->GetValue(), entry.type, entry.show_as_hex);
  float blink_alpha = CalculateBlinkAlpha(entry.data->GetLastChangeTime());

  if (blink_alpha > 0.0f) {
    ImVec4 default_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const auto color_red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 blink_color = LerpColor(default_color, color_red, blink_alpha);
    ImGui::PushStyleColor(ImGuiCol_Text, blink_color);
  }

  if (ImGui::InputText(
          "##value", &val_str, ImGuiInputTextFlags_EnterReturnsTrue)) {
    actions.on_value_changed(index, val_str);
  }

  if (blink_alpha > 0.0f) {
    ImGui::PopStyleColor();
  }
  RenderRowInteractions(entry);

  // Context Menu
  if (ImGui::BeginPopup("row_context")) {
    if (ImGui::MenuItem("Show as Hex", nullptr, entry.show_as_hex)) {
      actions.on_hex_display_toggled(index, !entry.show_as_hex);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Change Type")) {
      for (const auto& type : kAllScanValueTypes) {
        if (ImGui::MenuItem(
                ValueFormatter::GetLabel(type), nullptr, type == entry.type)) {
          actions.on_type_change_requested(index, type);
        }
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::Selectable("Delete")) {
      actions.on_delete_requested(index);
    }
    ImGui::EndPopup();
  }

  ImGui::PopID();
}

void RenderTable(const std::vector<CheatTableEntry>& entries,
                 const TableActions& actions) {
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

  if (ImGui::BeginTable("CheatTable", 5, kFlags)) {
    ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Description");
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < entries.size(); ++i) {
      RenderRow(entries[i], i, actions);
    }

    ImGui::EndTable();
  }
}

}  // namespace

void CheatTableView::RenderAddDialog(const AddDialogActions& actions) {
  if (!add_dialog_.show) {
    return;
  }

  ImGui::SetNextWindowPos(ImVec2(add_dialog_.pos_x, add_dialog_.pos_y),
                          ImGuiCond_Appearing);

  if (ImGui::Begin(
          "Add Address",
          &add_dialog_.show,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    ImGui::Text("Address (e.g., 0x1234 or game.exe+0x100):");
    ImGui::InputText("##addr", &add_dialog_.address_input);

    ImGui::Text("Type:");
    if (ImGui::BeginCombo("##type",
                          ValueFormatter::GetLabel(
                              kAllScanValueTypes[add_dialog_.type_index]))) {
      for (size_t i = 0; i < kAllScanValueTypes.size(); i++) {
        const bool is_selected = (add_dialog_.type_index == i);
        if (ImGui::Selectable(ValueFormatter::GetLabel(kAllScanValueTypes[i]),
                              is_selected)) {
          add_dialog_.type_index = static_cast<int>(i);
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::Text("Description:");
    ImGui::InputText("##desc", &add_dialog_.description_input);

    ImGui::Separator();

    if (ImGui::Button("Add", ImVec2(120, 0))) {
      if (!add_dialog_.address_input.empty()) {
        actions.on_add(add_dialog_.address_input,
                       kAllScanValueTypes[add_dialog_.type_index],
                       add_dialog_.description_input);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      actions.on_close();
    }

    ImGui::End();
  }
}

void CheatTableView::Render(const std::vector<CheatTableEntry>& entries) {
  if (ImGui::Begin("Cheat Table")) {
    RenderToolbar({
        .on_save = [this] { signals_.save_requested.publish(); },
        .on_load = [this] { signals_.load_requested.publish(); },
        .on_add_clicked =
            [this](ImVec2 pos) {
              add_dialog_.show = true;
              add_dialog_.address_input.clear();
              add_dialog_.description_input.clear();
              add_dialog_.type_index = 4;
              add_dialog_.pos_x = pos.x;
              add_dialog_.pos_y = pos.y;
            },
    });

    ImGui::Separator();

    RenderTable(entries,
                TableActions{
                    // clang-format off
                    .on_freeze_toggled =        [this](size_t i)                  { signals_.freeze_toggled.publish(i); },
                    .on_description_changed =   [this](size_t i, std::string d)   { signals_.description_changed.publish(i, d); },
                    .on_hex_display_toggled =   [this](size_t i, bool h)          { signals_.hex_display_toggled.publish(i, h); },
                    .on_type_change_requested = [this](size_t i, ScanValueType t) { signals_.type_change_requested.publish(i, t); },
                    .on_value_changed =         [this](size_t i, std::string v)   { signals_.value_changed.publish(i, v); },
                    .on_delete_requested =      [this](size_t i)                  { signals_.delete_requested.publish(i); },
                    // clang-format on
                });
  }
  ImGui::End();

  RenderAddDialog({
      .on_add =
          [this](std::string a, ScanValueType t, std::string d) {
            signals_.add_manual_requested.publish(a, t, d);
            add_dialog_.show = false;
          },
      .on_close = [this] { add_dialog_.show = false; },
  });
}

}  // namespace maia
