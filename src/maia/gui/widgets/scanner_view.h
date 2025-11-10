// Copyright (c) Maia

#pragma once

#include <variant>

#include <imgui.h>

#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"
#include "maia/core/scan_types.h"

namespace maia {

using ScanValueTypeSelection = std::variant<int8_t,
                                            uint8_t,
                                            int16_t,
                                            uint16_t,
                                            int32_t,
                                            uint32_t,
                                            int64_t,
                                            uint64_t,
                                            float,
                                            double>;

class ScannerWidget {
 public:
  class Signals {
   public:
    entt::sigh<void(std::vector<std::byte> value_to_scan)> new_scan_pressed;
    entt::sigh<void(std::vector<std::byte> value_to_scan)> scan_button_pressed;
    entt::sigh<void()> filter_changed;
    entt::sigh<void(ScanEntry)> entry_selected;
    entt::sigh<void(ScanValueTypeSelection)> value_type_selected;
  };

  void Render(const std::vector<ScanEntry>& entries);

  Signals& signals() {
    return signals_;
  }

 private:
  Signals signals_;

  std::string str_;
  int selected_index_ = 0;
  bool is_hex_input_ = false;
  int current_type_index_ = 4;  // Default to Int32 (index 4 in the combo)
};

}  // namespace maia
