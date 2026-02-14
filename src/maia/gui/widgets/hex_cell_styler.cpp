// Copyright (c) Maia

#include "maia/gui/widgets/hex_cell_styler.h"

#include <fmt/core.h>

#include "maia/gui/imgui_effects.h"

namespace maia::gui {

HexCellStyles HexCellStyler::GetStyles(const HexCellState& state) {
  HexCellStyles styles;

  if (state.is_pending) {
    styles.text = fmt::format("{:X}_", state.pending_nibble & 0xF);
  } else if (state.is_valid) {
    styles.text = fmt::format("{:02X}", static_cast<uint8_t>(state.value));
  } else {
    styles.text = "??";
  }

  // Text Colors
  if (state.is_edited) {
    styles.text_color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
  } else if (!state.is_valid) {
    styles.text_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
  } else {
    styles.text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Apply fade if changed recently and not edited/selected
    if (state.time_since_last_change < 2.0 && !state.is_selected) {
      auto t = static_cast<float>(state.time_since_last_change / 2.0);
      constexpr auto kRed = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
      constexpr auto kWhite = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      styles.text_color = LerpColor(kRed, kWhite, t);
    }
  }

  // Background Colors
  if (state.is_pending) {
    styles.bg_color = IM_COL32(255, 0, 0, 128);
  } else if (state.is_selected) {
    styles.bg_color =
        IM_COL32(66, 150, 250, 175);  // Approximate ImGuiCol_Header
  } else if (state.is_hovered) {
    styles.bg_color =
        IM_COL32(66, 150, 250, 102);  // Approximate ImGuiCol_FrameBgHovered
  } else {
    styles.bg_color = std::nullopt;
  }

  return styles;
}

}  // namespace maia::gui
