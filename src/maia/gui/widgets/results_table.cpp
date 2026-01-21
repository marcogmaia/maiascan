// Copyright (c) Maia

#include "maia/gui/widgets/results_table.h"

#include <format>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "maia/core/scan_types.h"

namespace maia {

namespace {

template <typename T>
void DrawFormattedValue(std::span<const std::byte> data, bool is_hex) {
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
      constexpr size_t kHexWidth = 2 * sizeof(T);
      ImGui::TextUnformatted(std::format("0x{:0{}x}", val, kHexWidth).c_str());
    } else {
      ImGui::TextUnformatted(std::format("{}", val).c_str());
    }
  }
}

// clang-format off
void DrawEntryByType(std::span<const std::byte> data,
                    ScanValueType type,
                    bool is_hex) {
  if (data.empty()) {
    ImGui::TextUnformatted("N/A");
    return;
  }

  switch (type) {
    case ScanValueType::kInt8:   DrawFormattedValue<int8_t>(data, is_hex); break;
    case ScanValueType::kUInt8:  DrawFormattedValue<uint8_t>(data, is_hex); break;
    case ScanValueType::kInt16:  DrawFormattedValue<int16_t>(data, is_hex); break;
    case ScanValueType::kUInt16: DrawFormattedValue<uint16_t>(data, is_hex); break;
    case ScanValueType::kInt32:  DrawFormattedValue<int32_t>(data, is_hex); break;
    case ScanValueType::kUInt32: DrawFormattedValue<uint32_t>(data, is_hex); break;
    case ScanValueType::kInt64:  DrawFormattedValue<int64_t>(data, is_hex); break;
    case ScanValueType::kUInt64: DrawFormattedValue<uint64_t>(data, is_hex); break;
    case ScanValueType::kFloat:  DrawFormattedValue<float>(data, false); break;
    case ScanValueType::kDouble: DrawFormattedValue<double>(data, false); break;
  }
}

// clang-format on

}  // namespace

void ResultsTable::Render(const ScanStorage& data,
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
        std::string addr_label = std::format("0x{:X}", data.addresses[i]);

        if (ImGui::Selectable(addr_label.c_str(),
                              selected_idx == i,
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
          selected_idx = i;
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            double_clicked = true;
          }
        }

        // Previous Value.
        ImGui::TableSetColumnIndex(1);
        if (prev_ptr) {
          std::span<const std::byte> prev_span(prev_ptr + offset, data.stride);
          DrawEntryByType(prev_span, value_type, is_hex);
        } else {
          ImGui::TextDisabled("-");
        }

        // Current Value.
        ImGui::TableSetColumnIndex(2);
        if (offset + data.stride <= data.curr_raw.size()) {
          std::span<const std::byte> val_span(curr_ptr + offset, data.stride);
          DrawEntryByType(val_span, value_type, is_hex);
        }
      }
    }
    ImGui::EndTable();
  }
  clipper.End();
}

}  // namespace maia
