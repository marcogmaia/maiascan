// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <array>
#include <cstring>
#include <format>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "maia/core/pattern_parser.h"
#include "maia/core/scan_types.h"
#include "maia/core/value_formatter.h"
#include "maia/core/value_parser.h"
#include "maia/gui/widgets/results_table.h"

namespace maia {

namespace {}  // namespace

void ScannerWidget::RenderControls(float progress, bool is_scanning) {
  if (!ImGui::Begin("Scanner")) {
    ImGui::End();
    return;
  }

  // Render Search Configuration (Type, Comparison, Input).
  if (ImGui::BeginTable("InputTable", 2)) {
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
      if (ImGui::BeginCombo("##ValueType",
                            ValueFormatter::GetLabel(
                                kAllScanValueTypes[current_type_index_]))) {
        for (size_t i = 0; i < kAllScanValueTypes.size(); i++) {
          const bool is_selected = std::cmp_equal(current_type_index_, i);
          if (ImGui::Selectable(ValueFormatter::GetLabel(kAllScanValueTypes[i]),
                                is_selected)) {
            current_type_index_ = static_cast<int>(i);
            const auto new_type = kAllScanValueTypes.at(current_type_index_);
            signals_.value_type_selected.publish(new_type);

            UpdateParsedValue();

            // Auto-select "Exact Value" for String/AOB types as other modes
            // don't usually make sense for a first scan.
            if (new_type == ScanValueType::kString ||
                new_type == ScanValueType::kWString ||
                new_type == ScanValueType::kArrayOfBytes) {
              selected_comparison_index_ = 1;  // "Exact Value"
              EmitSetComparisonSelected();
            }
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    });

    draw_row("Comparison:", [this]() {
      if (ImGui::BeginCombo(
              "##ScanComparison",
              ValueFormatter::GetLabel(
                  kAllScanComparisons[selected_comparison_index_]))) {
        for (int i = 0; i < kAllScanComparisons.size(); ++i) {
          const bool is_selected = selected_comparison_index_ == i;
          if (ImGui::Selectable(
                  ValueFormatter::GetLabel(kAllScanComparisons[i]),
                  is_selected)) {
            selected_comparison_index_ = i;
            EmitSetComparisonSelected();
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    });

    draw_row("Value:", [this]() {
      if (ImGui::InputText("##Input", &str_)) {
        UpdateParsedValue();
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
      const auto type = kAllScanValueTypes.at(current_type_index_);
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

    ImGui::EndDisabled();
    ImGui::EndTable();
  }

  if (ImGui::CollapsingHeader("Options")) {
    ImGui::BeginDisabled(is_scanning);
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
    ImGui::EndDisabled();
  }

  // Render Action Buttons.
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

  // Render Progress Bar when scanning.
  if (is_scanning) {
    ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0), "Scanning...");
    ImGui::Spacing();
  }

  // Shortcut hints
  if (ImGui::TreeNode("Shortcuts")) {
    ImGui::TextDisabled("Next Scan: Ctrl+Enter | New Scan: Ctrl+N");
    ImGui::TextDisabled(
        "Ctrl+Shift+C=Changed | U=Unchanged | +=Increased | -=Decreased | "
        "E=Exact");
    ImGui::TreePop();
  }

  ImGui::End();
}

void ScannerWidget::RenderResults(const ScanStorage& entries,
                                  const AddressFormatter& formatter) {
  if (!ImGui::Begin("Results")) {
    ImGui::End();
    return;
  }

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
    ScanValueType new_type = type;
    uintptr_t browse_address = 0;

    ResultsTableState state{
        .selected_idx = selected_index_,
        .double_clicked = double_clicked,
        .out_new_type = &new_type,
        .out_is_hex = &show_hex_results_,
        .out_browse_address = &browse_address,
    };

    table_renderer.Render(entries, formatter, type, show_hex_results_, state);

    if (new_type != type) {
      signals_.reinterpret_type_requested.publish(new_type);
      // Synchronize the combo box index
      for (size_t i = 0; i < kAllScanValueTypes.size(); ++i) {
        if (kAllScanValueTypes[i] == new_type) {
          current_type_index_ = static_cast<int>(i);
          break;
        }
      }
    }

    if (double_clicked) {
      signals_.entry_double_clicked.publish(selected_index_, type);
    }

    if (browse_address != 0) {
      signals_.browse_memory_requested.publish(browse_address);
    }
  }
  ImGui::EndChild();
  ImGui::End();
}

void ScannerWidget::EmitSetComparisonSelected() const {
  signals_.scan_comparison_selected.publish(
      kAllScanComparisons[selected_comparison_index_]);
}

void ScannerWidget::UpdateParsedValue() {
  const int base = is_hex_input_ ? 16 : 10;
  const auto type = kAllScanValueTypes.at(current_type_index_);

  auto pattern = ParsePatternByType(str_, type, base);

  parsed_preview_ = pattern.value;
  parse_error_ = pattern.value.empty() && !str_.empty();
  signals_.target_value_selected.publish(pattern.value, pattern.mask);
}

}  // namespace maia
