// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include <imgui.h>

namespace maia::gui {

struct HexCellStyles {
  ImVec4 text_color;
  std::optional<ImU32> bg_color;
  std::string text;
};

struct HexCellState {
  std::byte value;
  bool is_valid;
  bool is_edited;
  bool is_selected;
  bool is_hovered;
  bool is_pending;
  int pending_nibble;  // 0-15, only valid if is_pending is true
  double time_since_last_change = 1000.0;
};

class HexCellStyler {
 public:
  static HexCellStyles GetStyles(const HexCellState& state);
};

}  // namespace maia::gui
