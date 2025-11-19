// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <array>
#include <format>
#include <optional>
#include <string_view>

#include <imgui_stdlib.h>

#include "maia/logging.h"

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
void DrawEntryValue(const ScanEntry& entry,
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
void DrawEntryValue(const ScanEntry& entry) {
  auto value = *reinterpret_cast<const T*>(entry.data.data());
  ImGui::TextUnformatted(std::format("{:.6f}", value).c_str());
}

void DrawAddressValueEntry(const ScanEntry& entry,
                           ScanValueType type,
                           bool is_hexadecimal = false) {
  if (entry.data.size() < sizeof(uint32_t)) {
    ImGui::TextUnformatted("N/A");
    return;
  }

  // clang-format off
  switch (type) {
    case ScanValueType::kInt8:   { DrawEntryValue<int8_t>(entry, is_hexadecimal,    2); break; }
    case ScanValueType::kUInt8:  { DrawEntryValue<uint8_t>(entry, is_hexadecimal,   2); break; }
    case ScanValueType::kInt16:  { DrawEntryValue<int16_t>(entry, is_hexadecimal,   4); break; }
    case ScanValueType::kUInt16: { DrawEntryValue<uint16_t>(entry, is_hexadecimal,  4); break; }
    case ScanValueType::kInt32:  { DrawEntryValue<int32_t>(entry, is_hexadecimal,   8); break; }
    case ScanValueType::kUInt32: { DrawEntryValue<uint32_t>(entry, is_hexadecimal,  8); break; }
    case ScanValueType::kInt64:  { DrawEntryValue<int64_t>(entry, is_hexadecimal,  16); break; }
    case ScanValueType::kUInt64: { DrawEntryValue<uint64_t>(entry, is_hexadecimal, 16); break; }
    case ScanValueType::kFloat:  { DrawEntryValue<float>(entry);  break; }
    case ScanValueType::kDouble: { DrawEntryValue<double>(entry); break; }
  }
  // clang-format on
}

// Generic conversion function for any type
template <typename T>
std::vector<std::byte> GetBytes(const std::string& str, int base) {
  auto value = ParseValue<T>(str, base);
  return value ? ToByteVector(*value) : std::vector<std::byte>{};
}

std::vector<std::byte> ParseToBytes(const std::string& str,
                                    ScanValueType type,
                                    int base) {
  // clang-format off
  switch (type) {
    case ScanValueType::kInt8:   return GetBytes<int8_t>(str, base);
    case ScanValueType::kUInt8:  return GetBytes<uint8_t>(str, base);
    case ScanValueType::kInt16:  return GetBytes<int16_t>(str, base);
    case ScanValueType::kUInt16: return GetBytes<uint16_t>(str, base);
    case ScanValueType::kInt32:  return GetBytes<int32_t>(str, base);
    case ScanValueType::kUInt32: return GetBytes<uint32_t>(str, base);
    case ScanValueType::kInt64:  return GetBytes<int64_t>(str, base);
    case ScanValueType::kUInt64: return GetBytes<uint64_t>(str, base);
    case ScanValueType::kFloat:  return GetBytes<float>(str, base);
    case ScanValueType::kDouble: return GetBytes<double>(str, base);
    default:
      LogWarning("Unsupported format.");
      return {};
  }
  // clang-format on
}

constexpr std::array<ScanValueType, 10> kScanValueTypeByIndex = {
    ScanValueType::kInt8,
    ScanValueType::kUInt8,
    ScanValueType::kInt16,
    ScanValueType::kUInt16,
    ScanValueType::kInt32,
    ScanValueType::kUInt32,
    ScanValueType::kInt64,
    ScanValueType::kUInt64,
    ScanValueType::kFloat,
    ScanValueType::kDouble,
};

constexpr std::array<const char*, 10> kScanValueTypeLabels = {
    "Int8",
    "UInt8",
    "Int16",
    "UInt16",
    "Int32",
    "UInt32",
    "Int64",
    "UInt64",
    "Float",
    "Double",
};

// Add these arrays for scan comparison selection
constexpr std::array<const char*, 13> kScanComparisonLabels = {
    "Unknown",
    "Exact Value",
    "Not Equal",
    "Greater Than",
    "Less Than",
    "Between",
    "Not Between",
    "Changed",
    "Unchanged",
    "Increased",
    "Decreased",
    "Increased By",
    "Decreased By",
};

constexpr std::array<ScanComparison, 13> kScanComparisonByIndex = {
    ScanComparison::kUnknown,
    ScanComparison::kExactValue,
    ScanComparison::kNotEqual,
    ScanComparison::kGreaterThan,
    ScanComparison::kLessThan,
    ScanComparison::kBetween,
    ScanComparison::kNotBetween,
    ScanComparison::kChanged,
    ScanComparison::kUnchanged,
    ScanComparison::kIncreased,
    ScanComparison::kDecreased,
    ScanComparison::kIncreasedBy,
    ScanComparison::kDecreasedBy,
};

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
                       kScanValueTypeLabels.data(),
                       static_cast<int>(kScanValueTypeLabels.size()))) {
        // Type changed - could add validation here
      }
      ImGui::PopItemWidth();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Comparison:");
      ImGui::TableSetColumnIndex(1);
      ImGui::PushItemWidth(-FLT_MIN);
      if (ImGui::Combo("##ScanComparison",
                       &selected_comparison_index_,
                       kScanComparisonLabels.data(),
                       static_cast<int>(kScanComparisonLabels.size()))) {
        EmitSetComparisonSelected();
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

      const ScanValueType selected_type =
          kScanValueTypeByIndex.at(current_type_index_);
      auto needle_bytes = ParseToBytes(str_, selected_type, base);

      if (ImGui::Button("First Scan")) {
        signals_.new_scan_pressed.publish(needle_bytes);
      }
      ImGui::SameLine();
      if (ImGui::Button("Scan")) {
        signals_.next_scan_pressed.publish();
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
            DrawAddressValueEntry(entry, selected_type, is_hex_input_);
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

void ScannerWidget::EmitSetComparisonSelected() const {
  signals_.scan_comparison_selected.publish(
      kScanComparisonByIndex[selected_comparison_index_]);
}

}  // namespace maia
