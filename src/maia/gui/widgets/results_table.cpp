// Copyright (c) Maia

#include "maia/gui/widgets/results_table.h"

#include <algorithm>
#include <concepts>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "maia/core/address_formatter.h"
#include "maia/core/scan_types.h"
#include "maia/core/value_formatter.h"

namespace maia {

namespace {

// Helper to draw formatted values with optional color
void DrawFormattedValue(std::span<const std::byte> data,
                        ScanValueType type,
                        bool is_hex,
                        std::optional<ImVec4> color = std::nullopt) {
  std::string text = ValueFormatter::Format(data, type, is_hex);
  if (color) {
    ImGui::TextColored(*color, "%s", text.c_str());
  } else {
    ImGui::TextUnformatted(text.c_str());
  }
}

}  // namespace

void ResultsTable::Render(const ScanStorage& data,

                          const AddressFormatter& formatter,
                          ScanValueType value_type,
                          bool is_hex,
                          int& selected_idx,
                          bool& double_clicked) {
  // Here we use the clipper because the list may contain millions of results.
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(data.addresses.size()));

  double_clicked = false;

  constexpr int kNumCols = 3;
  if (ImGui::BeginTable("ScanResults",
                        kNumCols,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Previous");
    ImGui::TableSetupColumn("Current");
    ImGui::TableHeadersRow();

    const std::byte* curr_ptr = data.curr_raw.data();
    const std::byte* prev_ptr = nullptr;

    // Validate previous buffer existence/size.
    if (!data.prev_raw.empty() &&
        data.prev_raw.size() >= data.addresses.size() * data.stride) {
      prev_ptr = data.prev_raw.data();
    }

    // Render ONLY visible rows.
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        ImGui::TableNextRow();

        size_t offset = i * data.stride;

        // Address.
        ImGui::TableSetColumnIndex(0);
        auto formatted_addr = formatter.Format(data.addresses[i]);

        if (formatted_addr.is_relative) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }

        if (ImGui::Selectable(formatted_addr.text.c_str(),
                              selected_idx == i,
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
          selected_idx = i;
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            double_clicked = true;
          }
        }

        if (formatted_addr.is_relative) {
          ImGui::PopStyleColor();
        }

        // Previous Value.
        ImGui::TableSetColumnIndex(1);
        std::span<const std::byte> prev_span;
        if (prev_ptr) {
          prev_span =
              std::span<const std::byte>(prev_ptr + offset, data.stride);
          DrawFormattedValue(prev_span, value_type, is_hex);
        } else {
          ImGui::TextDisabled("-");
        }

        // Current Value.
        ImGui::TableSetColumnIndex(2);
        if (offset + data.stride <= data.curr_raw.size()) {
          std::span<const std::byte> curr_span(curr_ptr + offset, data.stride);

          std::optional<ImVec4> val_color;
          if (!prev_span.empty() && !std::ranges::equal(curr_span, prev_span)) {
            val_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
          }

          DrawFormattedValue(curr_span, value_type, is_hex, val_color);
        }
      }
    }
    ImGui::EndTable();
  }
  clipper.End();
}

}  // namespace maia
