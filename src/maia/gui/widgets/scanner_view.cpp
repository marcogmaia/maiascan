// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <array>
#include <charconv>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "maia/gui/widgets/results_table.h"

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
  std::vector<std::byte> bytes(sizeof(T));
  std::memcpy(bytes.data(), &value, sizeof(T));
  return bytes;
}

template <typename T>
std::vector<std::byte> NumberStrToBytes(const std::string& str, int base) {
  return ParseValue<T>(str, base)
      .transform(ToByteVector<T>)
      .value_or(std::vector<std::byte>{});
}

// clang-format off

std::vector<std::byte> ParseStringByType(const std::string& str,
                                         ScanValueType type,
                                         int base) {
  switch (type) {
    case ScanValueType::kInt8:   return NumberStrToBytes<int8_t>(str, base);
    case ScanValueType::kUInt8:  return NumberStrToBytes<uint8_t>(str, base);
    case ScanValueType::kInt16:  return NumberStrToBytes<int16_t>(str, base);
    case ScanValueType::kUInt16: return NumberStrToBytes<uint16_t>(str, base);
    case ScanValueType::kInt32:  return NumberStrToBytes<int32_t>(str, base);
    case ScanValueType::kUInt32: return NumberStrToBytes<uint32_t>(str, base);
    case ScanValueType::kInt64:  return NumberStrToBytes<int64_t>(str, base);
    case ScanValueType::kUInt64: return NumberStrToBytes<uint64_t>(str, base);
    case ScanValueType::kFloat:  return NumberStrToBytes<float>(str, base);
    case ScanValueType::kDouble: return NumberStrToBytes<double>(str, base);
    default: return {};
  }
}

// clang-format on

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

  // Render Search Configuration (Type, Comparison, Input).
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

    draw_row("Value:", [this]() {
      if (ImGui::InputText("##Input", &str_)) {
        const int base = is_hex_input_ ? 16 : 10;
        const auto type = kScanValueTypeByIndex.at(current_type_index_);
        auto bytes = ParseStringByType(str_, type, base);
        signals_.target_value_selected.publish(bytes);
      }
    });

    draw_row("Options:", [this]() {
      ImGui::Checkbox("Hex Input", &is_hex_input_);
      ImGui::SameLine();
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
      if (ImGui::Checkbox("Auto Update", &auto_update_enabled_)) {
        signals_.auto_update_changed.publish(auto_update_enabled_);
      }
    });

    ImGui::EndTable();
  };

  // Render Action Buttons.
  const auto render_actions = [this]() {
    ImGui::Separator();
    if (ImGui::Button("First Scan")) {
      signals_.new_scan_pressed.publish();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Scan")) {
      signals_.next_scan_pressed.publish();
    }
    ImGui::Separator();
  };

  // Execution Flow.
  render_search_options();
  render_actions();

  // Render the number of results found.
  const size_t total_count = entries.addresses.size();
  if (total_count > 0) {
    if (total_count > 10'000) {
      constexpr auto kWarningYellow = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
      ImGui::TextColored(
          kWarningYellow, "Found: %zu (Too many, please refine)", total_count);
    } else {
      ImGui::Text("Found: %zu", total_count);
    }
    ImGui::Spacing();
  } else {
    ImGui::TextDisabled("No results.");
  }
  ImGui::Separator();

  // Render Result Table.
  if (ImGui::BeginChild("Table")) {
    ResultsTable table_renderer;
    const auto type = kScanValueTypeByIndex.at(current_type_index_);
    table_renderer.Render(entries, type, is_hex_input_, selected_index_);
    ImGui::EndChild();
  }
  ImGui::End();
}

void ScannerWidget::EmitSetComparisonSelected() const {
  signals_.scan_comparison_selected.publish(
      kScanComparisonByIndex[selected_comparison_index_]);
}

}  // namespace maia
