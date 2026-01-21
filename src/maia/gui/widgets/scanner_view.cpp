// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <array>
#include <cstring>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "maia/core/value_parser.h"
#include "maia/gui/widgets/results_table.h"

namespace maia {

namespace {

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
    bool double_clicked = false;
    table_renderer.Render(
        entries, type, is_hex_input_, selected_index_, double_clicked);

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
