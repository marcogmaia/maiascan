// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <format>
#include <optional>
#include <string_view>

#include <imgui_stdlib.h>

namespace maia {

namespace {

template <typename T>
std::optional<T> ParseValue(std::string_view sview, int base = 10) {
  if (base == 16 && sview.starts_with("0x")) {
    sview = sview.substr(2);
  }
  const char* first = sview.data();
  const char* last = first + sview.size();

  T value;
  std::from_chars_result result;

  if constexpr (std::is_floating_point_v<T>) {
    result = std::from_chars(first, last, value);
  } else {
    result = std::from_chars(first, last, value, base);
  }

  if (result.ec != std::errc() || result.ptr != last) {
    return std::nullopt;
  }
  return value;
}

template <typename T>
std::vector<std::byte> ToByteVector(T value) {
  std::vector<std::byte> bytes(
      reinterpret_cast<std::byte*>(&value),
      reinterpret_cast<std::byte*>(&value) + sizeof(value));
  return bytes;
}

// Generic display function for integer types
template <typename T>
void DisplayIntegerValue(const ScanEntry& entry,
                         bool is_hexadecimal,
                         int hex_width) {
  auto value = *reinterpret_cast<const T*>(entry.data.data());
  if (is_hexadecimal) {
    ImGui::TextUnformatted(std::format("0x{:0{}x}", value, hex_width).c_str());
  } else {
    ImGui::TextUnformatted(std::format("{}", value).c_str());
  }
}

// Generic display function for floating-point types
template <typename T>
void DisplayFloatValue(const ScanEntry& entry) {
  auto value = *reinterpret_cast<const T*>(entry.data.data());
  ImGui::TextUnformatted(std::format("{:.6f}", value).c_str());
}

void TextEntryValue(const ScanEntry& entry,
                    ScanValueType type,
                    bool is_hexadecimal = false) {
  if (entry.data.size() < sizeof(uint32_t)) {
    ImGui::TextUnformatted("N/A");
    return;
  }

  switch (type) {
    case ScanValueType::kInt8:
      DisplayIntegerValue<int8_t>(entry, is_hexadecimal, 2);
      break;
    case ScanValueType::kUInt8:
      DisplayIntegerValue<uint8_t>(entry, is_hexadecimal, 2);
      break;
    case ScanValueType::kInt16:
      DisplayIntegerValue<int16_t>(entry, is_hexadecimal, 4);
      break;
    case ScanValueType::kUInt16:
      DisplayIntegerValue<uint16_t>(entry, is_hexadecimal, 4);
      break;
    case ScanValueType::kInt32:
      DisplayIntegerValue<int32_t>(entry, is_hexadecimal, 8);
      break;
    case ScanValueType::kUInt32:
      DisplayIntegerValue<uint32_t>(entry, is_hexadecimal, 8);
      break;
    case ScanValueType::kInt64:
      DisplayIntegerValue<int64_t>(entry, is_hexadecimal, 16);
      break;
    case ScanValueType::kUInt64:
      DisplayIntegerValue<uint64_t>(entry, is_hexadecimal, 16);
      break;
    case ScanValueType::kFloat:
      DisplayFloatValue<float>(entry);
      break;
    case ScanValueType::kDouble:
      DisplayFloatValue<double>(entry);
      break;
  }
}

// Generic conversion function for any type
template <typename T>
std::vector<std::byte> GetBytesForType(const std::string& str, int base) {
  auto value = ParseValue<T>(str, base);
  return value ? ToByteVector(*value) : std::vector<std::byte>{};
}

std::vector<std::byte> GetBytesFromString(const std::string& str,
                                          ScanValueType type,
                                          int base) {
  switch (type) {
    case ScanValueType::kInt8:
      return GetBytesForType<int8_t>(str, base);
    case ScanValueType::kUInt8:
      return GetBytesForType<uint8_t>(str, base);
    case ScanValueType::kInt16:
      return GetBytesForType<int16_t>(str, base);
    case ScanValueType::kUInt16:
      return GetBytesForType<uint16_t>(str, base);
    case ScanValueType::kInt32:
      return GetBytesForType<int32_t>(str, base);
    case ScanValueType::kUInt32:
      return GetBytesForType<uint32_t>(str, base);
    case ScanValueType::kInt64:
      return GetBytesForType<int64_t>(str, base);
    case ScanValueType::kUInt64:
      return GetBytesForType<uint64_t>(str, base);
    case ScanValueType::kFloat:
      return GetBytesForType<float>(str, base);
    case ScanValueType::kDouble:
      return GetBytesForType<double>(str, base);
  }
  return {};
}

}  // namespace

void ScannerWidget::Render(const std::vector<ScanEntry>& entries) {
  if (ImGui::Begin("Scanner")) {
    if (ImGui::BeginTable("InputTable", 2)) {
      ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Type:");

      ImGui::TableSetColumnIndex(1);
      ImGui::PushItemWidth(-FLT_MIN);
      if (ImGui::Combo("##ValueType",
                       &current_type_index_,
                       "Int8\0UInt8\0Int16\0UInt16\0Int32\0UInt32\0Int64\0UInt6"
                       "4\0Float\0Double\0\0")) {
        // Type changed - could add validation here
      }
      ImGui::PopItemWidth();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Value:");

      ImGui::TableSetColumnIndex(1);
      // Makes the input fill the cell.
      ImGui::PushItemWidth(-FLT_MIN);
      ImGui::InputText("##Input", &str_);
      ImGui::PopItemWidth();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Hex");

      ImGui::TableSetColumnIndex(1);
      ImGui::Checkbox("##HexInput", &is_hex_input_);

      ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::BeginChild("Table")) {
      const int base = is_hex_input_ ? 16 : 10;

      // Map combo index to ScanValueType
      ScanValueType selected_type =
          static_cast<ScanValueType>(current_type_index_);

      auto needle_bytes = GetBytesFromString(str_, selected_type, base);

      if (ImGui::Button("First Scan")) {
        signals_.new_scan_pressed.publish(needle_bytes);
      }
      ImGui::SameLine();
      if (ImGui::Button("Scan")) {
        signals_.scan_button_pressed.publish(needle_bytes);
      }

      ImGui::SameLine();
      if (ImGui::Button("Filter Changed")) {
        signals_.filter_changed.publish();
      }
      ImGui::SetItemTooltip("Filter out all values\nthat have been changed.");

      const ImGuiTableFlags flags = ImGuiTableFlags_RowBg;

      if (ImGui::BeginTable("Tab", 2, flags)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Value");

        ImGui::TableHeadersRow();

        for (int i = 0; i < entries.size(); ++i) {
          const auto& entry = entries[i];  // NOLINT
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          const bool is_selected = (selected_index_ == i);
          std::string address_str = std::format("0x{:x}", entry.address);

          // ImGuiSelectableFlags_SpanAllColumns makes it fill the whole row.
          if (ImGui::Selectable(address_str.c_str(),
                                is_selected,
                                ImGuiSelectableFlags_SpanAllColumns)) {
            selected_index_ = i;
            signals_.entry_selected.publish(entry);
          }

          ImGui::TableNextColumn();
          if (entry.data.size() >= sizeof(uint32_t)) {
            TextEntryValue(entry, selected_type, is_hex_input_);
          } else {
            ImGui::TextUnformatted("N/A");
          }
        }
        ImGui::EndTable();
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

}  // namespace maia
