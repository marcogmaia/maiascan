#pragma once

#include "maia/gui/models/hex_view_model.h"

namespace maia::gui {

class DataInspectorView {
 public:
  explicit DataInspectorView(HexViewModel& hex_view_model);

  void Render();

 private:
  HexViewModel& hex_view_model_;
};

}  // namespace maia::gui
