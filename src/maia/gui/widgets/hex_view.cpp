// Copyright (c) Maia

#include "maia/gui/widgets/hex_view.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>

#include <imgui.h>

#include "maia/core/string_utils.h"
#include "maia/gui/widgets/hex_cell_styler.h"

namespace maia::gui {

namespace {

void RenderAddress(uintptr_t address) {
  ImGui::PushStyleColor(ImGuiCol_Text,
                        ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  ImGui::TextUnformatted(core::FormatAddressHex(address).c_str());
  ImGui::PopStyleColor();
}

}  // namespace

HexView::HexView(HexViewModel& model)
    : model_(model),
      data_inspector_(model) {}

void HexView::Render() {
  model_.Refresh();
  row_height_ = ImGui::GetTextLineHeight();

  RenderToolbar();

  if (ImGui::BeginTable(
          "MainLayout",
          2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn(
        "HexGrid", ImGuiTableColumnFlags_WidthStretch, 0.7f);
    ImGui::TableSetupColumn(
        "Inspector", ImGuiTableColumnFlags_WidthStretch, 0.3f);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);

    if (ImGui::BeginChild("HexGridRegion",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      // Handle mouse wheel for scrolling
      if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
          model_.Scroll(static_cast<int32_t>(-wheel));
          model_.CachePage();
        }
      }

      // Auto-scroll when dragging near edges
      if (interaction_anchor_address_ != kInvalidAddress &&
          ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();
        float scroll_zone = row_height_ * 2.0f;

        if (mouse_pos.y < win_pos.y + scroll_zone) {
          model_.Scroll(-1);
          model_.CachePage();
        } else if (mouse_pos.y > win_pos.y + win_size.y - scroll_zone) {
          model_.Scroll(1);
          model_.CachePage();
        }
      }

      // Handle Input (Keyboard navigation and editing)
      HandleInput();

      RenderGrid();

      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        interaction_anchor_address_ = HexView::kInvalidAddress;
      }
    }
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    if (ImGui::BeginChild("InspectorRegion")) {
      data_inspector_.Render();
    }
    ImGui::EndChild();

    ImGui::EndTable();
  }
}

void HexView::RenderToolbar() {
  if (ImGui::Button("Go to...")) {
    ImGui::OpenPopup("GoToPopup");
  }
  ImGui::SameLine();
  if (ImGui::Button("Commit")) {
    model_.Commit();
    model_.CachePage();
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    model_.CachePage();
  }

  if (ImGui::BeginPopup("GoToPopup")) {
    ImGui::InputText("Address",
                     goto_addr_buffer_,
                     sizeof(goto_addr_buffer_),
                     ImGuiInputTextFlags_CharsHexadecimal);
    if (ImGui::Button("Go")) {
      uint64_t temp_addr = 0;
      auto [ptr, ec] =
          std::from_chars(goto_addr_buffer_,
                          goto_addr_buffer_ + std::strlen(goto_addr_buffer_),
                          temp_addr,
                          16);
      if (ec == std::errc() &&
          ptr == goto_addr_buffer_ + std::strlen(goto_addr_buffer_)) {
        model_.GoTo(static_cast<uintptr_t>(temp_addr));
        model_.CachePage();
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
}

void HexView::RenderGrid() {
  // Refresh cache if needed
  if (model_.GetCachedData().empty()) {
    model_.CachePage();
  }

  const auto& data = model_.GetCachedData();
  uintptr_t base_address = model_.GetCurrentAddress();

  // Layout constants
  const float glyph_width = ImGui::CalcTextSize("F").x;
  glyph_width_ = glyph_width;

  hovered_address_ = 0;  // Reset every frame

  // We use a table with 3 columns: Address, Hex, ASCII
  if (ImGui::BeginTable("HexViewTable",
                        3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn(
        "Offset", ImGuiTableColumnFlags_WidthFixed, glyph_width * 18.0f);
    ImGui::TableSetupColumn(
        "Hex", ImGuiTableColumnFlags_WidthFixed, glyph_width * 49.0f);
    ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    int total_rows =
        static_cast<int>((data.size() + kBytesPerRow - 1) / kBytesPerRow);

    ImGuiListClipper clipper;
    clipper.Begin(total_rows);

    while (clipper.Step()) {
      for (int row_idx = clipper.DisplayStart; row_idx < clipper.DisplayEnd;
           ++row_idx) {
        ImGui::TableNextRow();

        uintptr_t row_addr =
            base_address + static_cast<uintptr_t>(row_idx * kBytesPerRow);
        size_t data_offset = static_cast<int64_t>(row_idx) * kBytesPerRow;

        ImGui::TableSetColumnIndex(0);
        RenderAddress(row_addr);

        ImGui::TableSetColumnIndex(1);
        RenderHexBytes(row_addr, data, data_offset);

        ImGui::TableSetColumnIndex(2);
        RenderAscii(row_addr, data, data_offset);
      }
    }
    clipper.End();
    ImGui::EndTable();
  }
}

void HexView::RenderHexBytes(uintptr_t start_address,
                             const std::vector<std::byte>& data,
                             size_t data_offset) {
  const auto& validity = model_.GetValidityMask();
  const auto& edits = model_.GetEditBuffer();
  const auto& diff_map = model_.GetDiffMap();
  auto selection = model_.GetSelectionRange();
  auto now = std::chrono::steady_clock::now();

  for (int i = 0; i < kBytesPerRow; ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }

    if (data_offset + i >= data.size()) {
      break;
    }

    uintptr_t byte_addr = start_address + i;
    bool is_valid =
        (data_offset + i < validity.size()) && (validity[data_offset + i] != 0);

    auto val = std::byte{0};
    bool is_edited = false;

    if (auto it = edits.find(byte_addr); it != edits.end()) {
      val = it->second;
      is_edited = true;
      is_valid = true;
    } else if (is_valid) {
      val = data[data_offset + i];
    }

    bool is_selected =
        (byte_addr >= selection.start && byte_addr <= selection.end) &&
        (selection.start != HexView::kInvalidAddress ||
         selection.end != HexView::kInvalidAddress);
    bool is_hovered = (hovered_address_ == byte_addr);
    bool is_pending =
        (pending_nibble_ >= 0 && pending_nibble_addr_ == byte_addr);

    double time_since_last_change = 1000.0;
    if (auto it = diff_map.find(byte_addr); it != diff_map.end()) {
      std::chrono::duration<double> elapsed = now - it->second;
      time_since_last_change = elapsed.count();
    }

    HexCellState state{
        .value = val,
        .is_valid = is_valid,
        .is_edited = is_edited,
        .is_selected = is_selected,
        .is_hovered = is_hovered,
        .is_pending = is_pending,
        .pending_nibble = pending_nibble_,
        .time_since_last_change = time_since_last_change,
    };

    auto style = HexCellStyler::GetStyles(state);

    if (style.bg_color.has_value()) {
      ImVec2 pos = ImGui::GetCursorScreenPos();
      ImVec2 size = ImGui::CalcTextSize("FF");
      size.x += 4;
      ImGui::GetWindowDrawList()->AddRectFilled(
          pos, ImVec2(pos.x + size.x, pos.y + row_height_), *style.bg_color);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, style.text_color);
    ImGui::TextUnformatted(style.text.c_str());
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
      hovered_address_ = byte_addr;
    }

    HandleSelection(byte_addr);
  }
}

void HexView::RenderAscii(uintptr_t start_address,
                          const std::vector<std::byte>& data,
                          size_t data_offset) {
  const auto& validity = model_.GetValidityMask();
  const auto& edits = model_.GetEditBuffer();
  const auto& diff_map = model_.GetDiffMap();
  auto selection = model_.GetSelectionRange();
  auto now = std::chrono::steady_clock::now();

  for (int i = 0; i < kBytesPerRow; ++i) {
    if (i > 0) {
      ImGui::SameLine(0, 0);
    }

    if (data_offset + i >= data.size()) {
      break;
    }

    uintptr_t byte_addr = start_address + i;
    bool is_valid =
        (data_offset + i < validity.size()) && (validity[data_offset + i] != 0);

    auto val = std::byte{0};
    bool is_edited = false;

    if (auto it = edits.find(byte_addr); it != edits.end()) {
      val = it->second;
      is_edited = true;
      is_valid = true;
    } else if (is_valid) {
      val = data[data_offset + i];
    }

    bool is_selected =
        (byte_addr >= selection.start && byte_addr <= selection.end) &&
        (selection.start != HexView::kInvalidAddress ||
         selection.end != HexView::kInvalidAddress);
    bool is_hovered = (hovered_address_ == byte_addr);

    // Note: Ascii view does not show pending nibble state in original logic
    double time_since_last_change = 1000.0;
    if (auto it = diff_map.find(byte_addr); it != diff_map.end()) {
      std::chrono::duration<double> elapsed = now - it->second;
      time_since_last_change = elapsed.count();
    }

    HexCellState state{
        .value = val,
        .is_valid = is_valid,
        .is_edited = is_edited,
        .is_selected = is_selected,
        .is_hovered = is_hovered,
        .is_pending = false,
        .pending_nibble = -1,
        .time_since_last_change = time_since_last_change,
    };

    auto style = HexCellStyler::GetStyles(state);

    if (style.bg_color.has_value()) {
      ImVec2 pos = ImGui::GetCursorScreenPos();
      ImVec2 size = ImGui::CalcTextSize("A");
      ImGui::GetWindowDrawList()->AddRectFilled(
          pos, ImVec2(pos.x + size.x, pos.y + row_height_), *style.bg_color);
    }

    char c = '.';
    if (is_valid) {
      auto uc = static_cast<unsigned char>(val);
      if (std::isprint(uc)) {
        c = static_cast<char>(uc);
      }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, style.text_color);
    ImGui::Text("%c", c);
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
      hovered_address_ = byte_addr;
    }

    HandleSelection(byte_addr);
  }
}

void HexView::HandleSelection(uintptr_t address) {
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    if (ImGui::GetIO().KeyShift) {
      uintptr_t anchor = interaction_anchor_address_;
      if (anchor == kInvalidAddress) {
        auto selection = model_.GetSelectionRange();
        anchor = selection.start != kInvalidAddress ? selection.start : address;
      }
      uintptr_t start = std::min(anchor, address);
      uintptr_t end = std::max(anchor, address);
      model_.SetSelectionRange(start, end);
    } else {
      interaction_anchor_address_ = address;
      model_.SetSelectionRange(address, address);
      pending_nibble_ = -1;
    }
  } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
             ImGui::IsItemHovered()) {
    if (interaction_anchor_address_ != kInvalidAddress) {
      uintptr_t start = std::min(interaction_anchor_address_, address);
      uintptr_t end = std::max(interaction_anchor_address_, address);
      model_.SetSelectionRange(start, end);
    }
  }
}

void HexView::HandleInput() {
  if (!ImGui::IsWindowFocused()) {
    return;
  }

  auto selection = model_.GetSelectionRange();
  bool single_selection = (selection.start == selection.end &&
                           selection.start != HexView::kInvalidAddress);

  // Navigation
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
    model_.Scroll(-1);
    model_.CachePage();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
    model_.Scroll(1);
    model_.CachePage();
  }

  if (single_selection) {
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
      model_.SetSelectionRange(selection.start - 1, selection.start - 1);
      pending_nibble_ = -1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
      model_.SetSelectionRange(selection.start + 1, selection.start + 1);
      pending_nibble_ = -1;
    }
  }

  // Editing
  if (single_selection) {
    for (int n = 0; n < 16; n++) {
      if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + n)) ||
          (n >= 10 &&
           (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_A + n - 10))))) {
        if (pending_nibble_ < 0) {
          pending_nibble_ = n;
          pending_nibble_addr_ = selection.start;
        } else {
          auto val = static_cast<std::byte>((pending_nibble_ << 4) | n);
          model_.SetByte(selection.start, val);
          pending_nibble_ = -1;
          // Auto-advance
          model_.SetSelectionRange(selection.start + 1, selection.start + 1);
        }
      }
    }
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
    model_.Commit();
    model_.CachePage();
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    pending_nibble_ = -1;
  }
}

}  // namespace maia::gui
