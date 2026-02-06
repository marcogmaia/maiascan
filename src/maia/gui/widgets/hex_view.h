#pragma once

#include <cstdint>
#include <vector>

#include "maia/gui/models/hex_view_model.h"
#include "maia/gui/widgets/data_inspector_view.h"

namespace maia::gui {

class HexView {
 public:
  explicit HexView(HexViewModel& model);

  void Render();

 private:
  void RenderToolbar();
  void RenderGrid();
  void HandleInput();
  void HandleSelection(uintptr_t address);
  void RenderAddress(uintptr_t address);
  void RenderHexBytes(uintptr_t start_address,
                      const std::vector<std::byte>& row_data,
                      size_t row_index);
  void RenderAscii(uintptr_t start_address,
                   const std::vector<std::byte>& row_data,
                   size_t row_index);

  HexViewModel& model_;
  DataInspectorView data_inspector_;
  float row_height_ = 0.0f;
  float glyph_width_ = 0.0f;
  char goto_addr_buffer_[32] = "";

  static constexpr uintptr_t kInvalidAddress = ~0ULL;
  static constexpr int kBytesPerRow = 16;

  // State for interaction
  uintptr_t hovered_address_ = 0;
  uintptr_t interaction_anchor_address_ = kInvalidAddress;
  int pending_nibble_ = -1;
  uintptr_t pending_nibble_addr_ = 0;
};

}  // namespace maia::gui
