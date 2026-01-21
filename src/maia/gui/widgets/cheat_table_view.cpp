// Copyright (c) Maia

#include "maia/gui/widgets/cheat_table_view.h"

#include <imgui.h>
#include <imgui_stdlib.h>  // for InputText with std::string
#include <format>
#include <mutex>

namespace maia {

namespace {

// Helper to format value for display
template <typename T>
std::string FormatValue(const std::vector<std::byte>& data) {
  if (data.size() < sizeof(T)) {
    return "Invalid";
  }
  T val;
  std::memcpy(&val, data.data(), sizeof(T));
  if constexpr (std::is_floating_point_v<T>) {
    return std::format("{:.4f}", val);
  } else {
    return std::format("{}", val);
  }
}

std::string GetValueString(ScanValueType type,
                           const std::vector<std::byte>& data) {
  // clang-format off
  switch (type) {
    case ScanValueType::kInt8: return FormatValue<int8_t>(data);
    case ScanValueType::kUInt8: return FormatValue<uint8_t>(data);
    case ScanValueType::kInt16: return FormatValue<int16_t>(data);
    case ScanValueType::kUInt16: return FormatValue<uint16_t>(data);
    case ScanValueType::kInt32: return FormatValue<int32_t>(data);
    case ScanValueType::kUInt32: return FormatValue<uint32_t>(data);
    case ScanValueType::kInt64: return FormatValue<int64_t>(data);
    case ScanValueType::kUInt64: return FormatValue<uint64_t>(data);
    case ScanValueType::kFloat: return FormatValue<float>(data);
    case ScanValueType::kDouble: return FormatValue<double>(data);
  }
  // clang-format on
  return "?";
}

const char* GetTypeString(ScanValueType type) {
  // clang-format off
  switch (type) {
    case ScanValueType::kInt8: 
    case ScanValueType::kUInt8:  return "1 Byte";
    case ScanValueType::kInt16:
    case ScanValueType::kUInt16: return "2 Bytes";
    case ScanValueType::kInt32:
    case ScanValueType::kUInt32: return "4 Bytes";
    case ScanValueType::kInt64:
    case ScanValueType::kUInt64: return "8 Bytes";
    case ScanValueType::kFloat:  return "Float";
    case ScanValueType::kDouble: return "Double";
  }
  // clang-format on
  return "Unknown";
}

}  // namespace

void CheatTableView::Render(const std::vector<CheatTableEntry>& entries) {
  if (ImGui::Begin("Cheat Table")) {
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

        {
          std::scoped_lock entry_lock(entry.data->mutex);
          val_str = GetValueString(entry.type, entry.data->value);
          is_frozen = entry.data->is_frozen;
        }

        // 1. Frozen Checkbox
        ImGui::TableSetColumnIndex(0);
        bool frozen = is_frozen;
        if (ImGui::Checkbox("##frozen", &frozen)) {
          signals_.freeze_toggled.publish(i);
        }

        // 2. Description (Editable)
        ImGui::TableSetColumnIndex(1);
        // Using InputText for direct editing could be tricky inside a table if
        // not careful with focus. Simple implementation: InputText
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
        ImGui::Text("0x%llX", entry.address);

        // 4. Type (Read-only for now)
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%s", GetTypeString(entry.type));

        // 5. Value (Editable)
        ImGui::TableSetColumnIndex(4);
        ImGui::SetNextItemWidth(-FLT_MIN);
        // Unique ID based on index
        if (ImGui::InputText(
                "##value", &val_str, ImGuiInputTextFlags_EnterReturnsTrue)) {
          signals_.value_changed.publish(i, val_str);
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
  }
  ImGui::End();
}

}  // namespace maia
