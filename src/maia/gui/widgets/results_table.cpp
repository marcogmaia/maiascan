// Copyright (c) Maia

#include "maia/gui/widgets/results_table.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "maia/core/address_formatter.h"
#include "maia/core/scan_types.h"
#include "maia/core/value_formatter.h"
#include "maia/gui/widgets/results_table_logic.h"

namespace maia {

struct ResultsTable::Context {
  const ScanStorage& data;
  const AddressFormatter& formatter;
  ScanValueType value_type;
  bool is_hex;
  ResultsTableState& state;

  const std::byte* curr_ptr = nullptr;
  const std::byte* prev_ptr = nullptr;
};

namespace {

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
                          ResultsTableState& state) {
  Context ctx{
      .data = data,
      .formatter = formatter,
      .value_type = value_type,
      .is_hex = is_hex,
      .state = state,
      .curr_ptr = data.curr_raw.data(),
  };

  if (!data.prev_raw.empty() &&
      data.prev_raw.size() >= data.addresses.size() * data.stride) {
    ctx.prev_ptr = data.prev_raw.data();
  }

  state.double_clicked = false;

  constexpr int kNumCols = 3;
  if (ImGui::BeginTable("ScanResults",
                        kNumCols,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Previous");
    ImGui::TableSetupColumn("Current");
    ImGui::TableHeadersRow();

    RenderContextMenu(ctx);
    RenderRows(ctx);

    ImGui::EndTable();
  }
}

void ResultsTable::RenderContextMenu(Context& ctx) {
  if (!ImGui::BeginPopupContextWindow("ResultsTableContext")) {
    return;
  }

  if (ImGui::MenuItem("Browse Memory")) {
    if (ctx.state.out_browse_address && ctx.state.selected_idx >= 0 &&
        std::cmp_less(ctx.state.selected_idx, ctx.data.addresses.size())) {
      *ctx.state.out_browse_address =
          ctx.data.addresses[ctx.state.selected_idx];
    }
  }

  RenderReinterpretMenu(ctx);
  RenderHexToggle(ctx);

  ImGui::EndPopup();
}

void ResultsTable::RenderReinterpretMenu(Context& ctx) {
  if (!ImGui::BeginMenu("Reinterpret Results As")) {
    return;
  }

  for (const auto type : kAllScanValueTypes) {
    const bool selected = (type == ctx.value_type);
    if (ImGui::MenuItem(ValueFormatter::GetLabel(type), nullptr, selected)) {
      if (ctx.state.out_new_type) {
        *ctx.state.out_new_type = type;
      }
    }
  }
  ImGui::EndMenu();
}

void ResultsTable::RenderHexToggle(Context& ctx) {
  if (ImGui::MenuItem("Show Values as Hex", nullptr, ctx.is_hex)) {
    if (ctx.state.out_is_hex) {
      *ctx.state.out_is_hex = !ctx.is_hex;
    }
  }
}

void ResultsTable::RenderRows(Context& ctx) {
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(ctx.data.addresses.size()));

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
      RenderSingleRow(i, ctx);
    }
  }
  clipper.End();
}

void ResultsTable::RenderSingleRow(int i, Context& ctx) {
  ImGui::TableNextRow();

  const size_t offset = i * ctx.data.stride;
  const uintptr_t address = ctx.data.addresses[i];

  // Address Column
  ImGui::TableSetColumnIndex(0);
  const auto formatted = ctx.formatter.Format(address);

  if (formatted.is_relative) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
  }

  const bool is_selected = (ctx.state.selected_idx == i);
  if (ImGui::Selectable(formatted.text.c_str(),
                        is_selected,
                        ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowDoubleClick)) {
    ctx.state.selected_idx = i;
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      ctx.state.double_clicked = true;
    }
  }

  if (formatted.is_relative) {
    ImGui::PopStyleColor();
  }

  // Previous Column
  ImGui::TableSetColumnIndex(1);
  std::span<const std::byte> prev_span;
  if (ctx.prev_ptr) {
    prev_span = {ctx.prev_ptr + offset, ctx.data.stride};
    DrawFormattedValue(prev_span, ctx.value_type, ctx.is_hex);
  } else {
    ImGui::TextDisabled("-");
  }

  // Current Column
  ImGui::TableSetColumnIndex(2);
  if (offset + ctx.data.stride <= ctx.data.curr_raw.size()) {
    std::span<const std::byte> curr_span{ctx.curr_ptr + offset,
                                         ctx.data.stride};

    std::optional<ImVec4> val_color;
    if (ResultsTableLogic::ShouldHighlightValue(curr_span, prev_span)) {
      val_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    DrawFormattedValue(curr_span, ctx.value_type, ctx.is_hex, val_color);
  }
}

}  // namespace maia
