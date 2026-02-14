// Copyright (c) Maia

#pragma once

#include <cstdint>

#include "maia/application/process_model.h"
#include "maia/gui/models/hex_view_model.h"

namespace maia {

class HexViewViewModel {
 public:
  HexViewViewModel(ProcessModel& process_model, gui::HexViewModel& hex_model);

  bool IsVisible() const {
    return is_visible_;
  }

  void SetVisible(bool visible) {
    is_visible_ = visible;
  }

  void ToggleVisibility() {
    is_visible_ = !is_visible_;
  }

  void GoToAddress(uintptr_t address);

 private:
  ProcessModel& process_model_;
  gui::HexViewModel& hex_model_;
  bool is_visible_ = false;
};

}  // namespace maia
