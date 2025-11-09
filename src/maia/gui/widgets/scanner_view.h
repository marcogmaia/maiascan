// Copyright (c) Maia

#pragma once

#include <variant>

#include <imgui.h>

#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"
#include "maia/core/scan_types.h"

namespace maia {

using ScanValueTypeSelection = std::variant<ScanValueType<int8_t>,
                                            ScanValueType<uint8_t>,
                                            ScanValueType<int16_t>,
                                            ScanValueType<uint16_t>,
                                            ScanValueType<int32_t>,
                                            ScanValueType<uint32_t>,
                                            ScanValueType<int64_t>,
                                            ScanValueType<uint64_t>,
                                            ScanValueType<float>,
                                            ScanValueType<double>>;

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
  int selected_index_;
  bool is_hex_input_;
};

}  // namespace maia
