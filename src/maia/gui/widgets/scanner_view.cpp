// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <array>
#include <charconv>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <imgui_stdlib.h>

namespace maia {

namespace {

// --- Helpers -----------------------------------------------------------------

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
  std::vector<std::byte> bytes(sizeof(T));
  std::memcpy(bytes.data(), &value, sizeof(T));
  return bytes;
}

template <typename T>
std::vector<std::byte> GetBytes(const std::string& str, int base) {
  auto value = ParseValue<T>(str, base);
  return value ? ToByteVector(*value) : std::vector<std::byte>{};
}

template <typename T>
void DrawFormattedValue(std::span<const std::byte> data,
                        bool is_hex,
                        int hex_width) {
  if (data.size() < sizeof(T)) {
    ImGui::TextUnformatted("Invalid");
    return;
  }
  T val;
  std::memcpy(&val, data.data(), sizeof(T));

  if constexpr (std::is_floating_point_v<T>) {
    ImGui::TextUnformatted(std::format("{:.6f}", val).c_str());
  } else {
    if (is_hex) {
      ImGui::TextUnformatted(std::format("0x{:0{}x}", val, hex_width).c_str());
    } else {
      ImGui::TextUnformatted(std::format("{}", val).c_str());
    }
  }
}

// --- Dispatchers -------------------------------------------------------------

// clang-format off

void DrawEntryByType(std::span<const std::byte> data,
                     ScanValueType type,
                     bool is_hex) {
  if (data.empty()) {
    ImGui::TextUnformatted("N/A");
    return;
  }

  switch (type) {
    case ScanValueType::kInt8:   DrawFormattedValue<int8_t>(data, is_hex, 2); break;
    case ScanValueType::kUInt8:  DrawFormattedValue<uint8_t>(data, is_hex, 2); break;
    case ScanValueType::kInt16:  DrawFormattedValue<int16_t>(data, is_hex, 4); break;
    case ScanValueType::kUInt16: DrawFormattedValue<uint16_t>(data, is_hex, 4); break;
    case ScanValueType::kInt32:  DrawFormattedValue<int32_t>(data, is_hex, 8); break;
    case ScanValueType::kUInt32: DrawFormattedValue<uint32_t>(data, is_hex, 8); break;
    case ScanValueType::kInt64:  DrawFormattedValue<int64_t>(data, is_hex, 16); break;
    case ScanValueType::kUInt64: DrawFormattedValue<uint64_t>(data, is_hex, 16); break;
    case ScanValueType::kFloat:  DrawFormattedValue<float>(data, false, 0); break;
    case ScanValueType::kDouble: DrawFormattedValue<double>(data, false, 0); break;
  }
}

std::vector<std::byte> ParseStringByType(const std::string& str,
                                         ScanValueType type,
                                         int base) {
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
    default: return {};
  }
}

// clang-format on

// --- Constants ---------------------------------------------------------------

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

void ScannerWidget::Render(const ScanStorage& entries) {
  if (!ImGui::Begin("Scanner")) {
    ImGui::End();
    return;
  }

  // Render Search Configuration (Type, Comparison, Input)
  const auto render_search_options = [this]() {
    if (!ImGui::BeginTable("InputTable", 2)) {
      return;
    }

    ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);

    const auto draw_row = [](const char* label, auto&& widget_fn) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(label);
      ImGui::TableSetColumnIndex(1);
      ImGui::PushItemWidth(-FLT_MIN);
      widget_fn();
      ImGui::PopItemWidth();
    };

    draw_row("Type:", [this]() {
      ImGui::Combo("##ValueType",
                   &current_type_index_,
                   kScanValueTypeLabels.data(),
                   static_cast<int>(kScanValueTypeLabels.size()));
    });

    draw_row("Comparison:", [this]() {
      if (ImGui::Combo("##ScanComparison",
                       &selected_comparison_index_,
                       kScanComparisonLabels.data(),
                       static_cast<int>(kScanComparisonLabels.size()))) {
        EmitSetComparisonSelected();
      }
    });

    draw_row("Value:", [this]() { ImGui::InputText("##Input", &str_); });
    draw_row("Hex:",
             [this]() { ImGui::Checkbox("##HexInput", &is_hex_input_); });

    ImGui::EndTable();
  };

  // Render Action Buttons (First Scan, Next Scan)
  const auto render_actions = [this]() {
    const int base = is_hex_input_ ? 16 : 10;
    const auto type = kScanValueTypeByIndex.at(current_type_index_);

    ImGui::Separator();
    if (ImGui::Button("First Scan")) {
      auto bytes = ParseStringByType(str_, type, base);
      signals_.new_scan_pressed.publish(bytes);
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Scan")) {
      signals_.next_scan_pressed.publish();
    }
    ImGui::Separator();
  };

  // Render Results Table
  const auto render_results = [this, &entries]() {
    size_t total_count = 0;
    total_count += entries.addresses.size();

    if (total_count > 2000) {
      ImGui::Text("Values found: %zu", total_count);
      return;
    }

    if (!ImGui::BeginChild("Table")) {
      return;
    }

    if (ImGui::BeginTable("Tab", 2, ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Address");
      ImGui::TableSetupColumn("Value");
      ImGui::TableHeadersRow();

      int global_idx = 0;
      const auto type = kScanValueTypeByIndex.at(current_type_index_);

      const std::byte* val_ptr = entries.raw_values_buffer.data();

      for (size_t i = 0; i < entries.addresses.size(); ++i) {
        ImGui::TableNextRow();

        // Address Column
        ImGui::TableNextColumn();
        std::string addr_label = std::format("0x{:x}", entries.addresses[i]);
        bool is_sel = (selected_index_ == global_idx);

        if (ImGui::Selectable(addr_label.c_str(),
                              is_sel,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          selected_index_ = global_idx;
          // signals_.entry_selected.publish(...);
        }

        // Value Column
        ImGui::TableNextColumn();
        std::span<const std::byte> val_span(val_ptr + (i * entries.stride),
                                            entries.stride);
        DrawEntryByType(val_span, type, is_hex_input_);

        global_idx++;
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();
  };

  // Execution Flow
  render_search_options();
  render_actions();
  render_results();

  ImGui::End();
}

void ScannerWidget::EmitSetComparisonSelected() const {
  signals_.scan_comparison_selected.publish(
      kScanComparisonByIndex[selected_comparison_index_]);
}

}  // namespace maia
