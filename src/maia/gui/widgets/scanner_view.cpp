// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <array>
#include <cstring>
#include <format>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "maia/core/pattern_parser.h"
#include "maia/core/value_parser.h"
#include "maia/gui/widgets/results_table.h"

namespace maia {

namespace {

constexpr auto kScanValueTypeByIndex = std::array{
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
    ScanValueType::kString,
    ScanValueType::kWString,
    ScanValueType::kArrayOfBytes,
};

constexpr auto kScanValueTypeLabels = std::array{
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
    "String",
    "Unicode String",
    "Array of Bytes",
};

constexpr auto kScanComparisonLabels = std::array{
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

constexpr auto kScanComparisonByIndex = std::array{
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

// Recursive/Fold step: Split first arg from the rest
template <typename First, typename... Rest>
constexpr bool AllSizesEqual(First&& first, Rest&&... rest) {
  // Check if every element in 'rest' has the same size as 'first'
  return ((rest.size() == first.size()) && ...);
}

static_assert(AllSizesEqual(kScanValueTypeByIndex,
                            kScanValueTypeLabels,
                            kScanComparisonLabels,
                            kScanValueTypeByIndex));

}  // namespace

void ScannerWidget::Render(const ScanStorage& entries,
                           const AddressFormatter& formatter,
                           float progress,
                           bool is_scanning) {
  if (!ImGui::Begin("Scanner")) {
    ImGui::End();
    return;
  }

  // Render Search Configuration (Type, Comparison, Input).
  const auto render_search_options = [this, is_scanning]() {
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

    ImGui::BeginDisabled(is_scanning);

    draw_row("Type:", [this]() {
      if (ImGui::Combo("##ValueType",
                       &current_type_index_,
                       kScanValueTypeLabels.data(),
                       static_cast<int>(kScanValueTypeLabels.size()))) {
        const auto new_type = kScanValueTypeByIndex.at(current_type_index_);
        signals_.value_type_selected.publish(new_type);

        // Re-parse and publish the current string with the new type
        const int base = is_hex_input_ ? 16 : 10;
        std::vector<std::byte> value;
        std::vector<std::byte> mask;

        if (new_type == ScanValueType::kArrayOfBytes) {
          auto p = ParseAob(str_);
          value = std::move(p.value);
          mask = std::move(p.mask);
        } else if (new_type == ScanValueType::kString) {
          auto p = ParseText(str_, false);
          value = std::move(p.value);
          mask = std::move(p.mask);
        } else if (new_type == ScanValueType::kWString) {
          auto p = ParseText(str_, true);
          value = std::move(p.value);
          mask = std::move(p.mask);
        } else {
          value = ParseStringByType(str_, new_type, base);
        }

        parsed_preview_ = value;
        parse_error_ = value.empty() && !str_.empty();
        signals_.target_value_selected.publish(value, mask);

        // Auto-select "Exact Value" for String/AOB types as other modes don't
        // usually make sense for a first scan.
        if (new_type == ScanValueType::kString ||
            new_type == ScanValueType::kWString ||
            new_type == ScanValueType::kArrayOfBytes) {
          selected_comparison_index_ = 1;  // "Exact Value"
          EmitSetComparisonSelected();
        }
      }
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

        std::vector<std::byte> value;
        std::vector<std::byte> mask;

        if (type == ScanValueType::kArrayOfBytes) {
          auto p = ParseAob(str_);
          value = std::move(p.value);
          mask = std::move(p.mask);
        } else if (type == ScanValueType::kString) {
          auto p = ParseText(str_, false);
          value = std::move(p.value);
          mask = std::move(p.mask);
        } else if (type == ScanValueType::kWString) {
          auto p = ParseText(str_, true);
          value = std::move(p.value);
          mask = std::move(p.mask);
        } else {
          value = ParseStringByType(str_, type, base);
        }

        parsed_preview_ = value;
        parse_error_ = value.empty() && !str_.empty();
        signals_.target_value_selected.publish(value, mask);
      }
    });

    if (parse_error_) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                         "Invalid input pattern!");
    }

    // Debug Feedback: Show what the parser sees
    if (!parsed_preview_.empty()) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(1);
      const auto type = kScanValueTypeByIndex.at(current_type_index_);
      std::string label = "Preview: ";
      if (type == ScanValueType::kString) {
        label = "Preview (UTF-8): ";
      } else if (type == ScanValueType::kWString) {
        label = "Preview (UTF-16): ";
      } else if (type == ScanValueType::kArrayOfBytes) {
        label = "Preview (AOB): ";
      }

      std::string hex_str = label;
      size_t count = 0;
      for (auto b : parsed_preview_) {
        hex_str += std::format("{:02X} ", static_cast<uint8_t>(b));
        if (++count >= 16) {
          if (parsed_preview_.size() > 16) {
            hex_str += "...";
          }
          break;
        }
      }
      ImGui::TextDisabled("%s", hex_str.c_str());
    }

    draw_row("Options:", [this]() {
      ImGui::Checkbox("Hex Input", &is_hex_input_);
      ImGui::SameLine();
      if (ImGui::Checkbox("Auto Update", &auto_update_enabled_)) {
        signals_.auto_update_changed.publish(auto_update_enabled_);
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Pause while scanning",
                          &pause_while_scanning_enabled_)) {
        signals_.pause_while_scanning_changed.publish(
            pause_while_scanning_enabled_);
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Fast Scan", &fast_scan_enabled_)) {
        signals_.fast_scan_changed.publish(fast_scan_enabled_);
      }
    });

    ImGui::EndDisabled();

    ImGui::EndTable();
  };

  // Render Action Buttons.
  const auto render_actions = [this, is_scanning]() {
    ImGui::Separator();

    ImGui::BeginDisabled(is_scanning);
    if (ImGui::Button("First Scan")) {
      signals_.new_scan_pressed.publish();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Scan")) {
      signals_.next_scan_pressed.publish();
    }
    ImGui::EndDisabled();

    if (is_scanning) {
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        signals_.cancel_scan_pressed.publish();
      }
    }

    ImGui::Separator();
  };

  // Render Progress Bar when scanning.
  const auto render_progress = [progress, is_scanning]() {
    if (!is_scanning) {
      return;
    }
    ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0), "Scanning...");
    ImGui::Spacing();
  };

  // Execution Flow.
  render_search_options();
  render_actions();
  render_progress();

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
    const auto type = entries.value_type;
    bool double_clicked = false;
    table_renderer.Render(entries,
                          formatter,
                          type,
                          is_hex_input_,
                          selected_index_,
                          double_clicked);

    if (double_clicked) {
      signals_.entry_double_clicked.publish(selected_index_, type);
    }

    ImGui::EndChild();
  }
  ImGui::End();
}

void ScannerWidget::EmitSetComparisonSelected() const {
  signals_.scan_comparison_selected.publish(
      kScanComparisonByIndex[selected_comparison_index_]);
}

}  // namespace maia
