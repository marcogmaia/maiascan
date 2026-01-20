// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>
#include <string>
#include <vector>

#include "maia/core/scan_types.h"

namespace maia {

class ScannerWidget {
 public:
  void Render(const ScanStorage& entries);

  auto sinks() {
    return Sinks{*this};
  }

 private:
  class Signals {
   public:
    entt::sigh<void()> new_scan_pressed;
    entt::sigh<void()> next_scan_pressed;
    // entt::sigh<void(std::vector<std::byte> value_to_scan)>
    // scan_button_pressed;
    entt::sigh<void(std::vector<std::byte>)> target_value_selected;
    entt::sigh<void(ScanComparison)> scan_comparison_selected;
    entt::sigh<void(bool)> auto_update_changed;
    entt::sigh<void(int, ScanValueType)> entry_double_clicked;

    // TODO: Define a new structure or method to pass selection data
    // entt::sigh<void(ScanStorage)> entry_selected;
    // entt::sigh<void(std::vector<MemoryAddress>, std::vector<std::byte>)>
    //     set_scan_value;
  };

  // clang-format off
  struct Sinks {
    ScannerWidget& view;
    auto NewScanPressed() {return entt::sink(view.signals_.new_scan_pressed);}
    auto NextScanPressed() {return entt::sink(view.signals_.next_scan_pressed);}
    auto TargetValueSelected() {return entt::sink(view.signals_.target_value_selected);}
    auto ScanComparisonSelected() {return entt::sink(view.signals_.scan_comparison_selected);}
    auto AutoUpdateChanged() {return entt::sink(view.signals_.auto_update_changed);}
    auto EntryDoubleClicked() {return entt::sink(view.signals_.entry_double_clicked);}
  };

  // clang-format on

  void EmitSetComparisonSelected() const;

  Signals signals_;

  std::string str_;
  int selected_index_ = 0;
  bool is_hex_input_ = false;
  bool auto_update_enabled_ = false;
  int current_type_index_ = static_cast<int>(ScanValueType::kInt32);
  int selected_comparison_index_ = static_cast<int>(ScanComparison::kChanged);
};

}  // namespace maia
