// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"

namespace maia {

class ScannerWidget {
 public:
  class Signals {
   public:
    entt::sigh<void(std::vector<std::byte> value_to_scan)> new_scan_pressed;
    entt::sigh<void(std::vector<std::byte> value_to_scan)> scan_button_pressed;
    entt::sigh<void()> filter_changed;
    entt::sigh<void(ScanEntry)> entry_selected;
  };

  void Render(const std::vector<ScanEntry>& entries);

  Signals& signals() {
    return signals_;
  }

 private:
  Signals signals_;

  std::string str_;
  int selected_index_;
  bool is_hex_input_;
};

}  // namespace maia
