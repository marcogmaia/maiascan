// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>
#include <string>
#include <vector>

#include "maia/application/scan_result_model.h"
#include "maia/core/scan_types.h"

namespace maia {

class ScannerWidget {
 public:
  class Signals {
   public:
    entt::sigh<void(std::vector<std::byte> value_to_scan)> new_scan_pressed;
    // entt::sigh<void(std::vector<std::byte> value_to_scan)>
    // scan_button_pressed;
    entt::sigh<void()> next_scan_pressed;
    entt::sigh<void(ScanComparison)> scan_comparison_selected;

    // TODO: Define a new structure or method to pass selection data
    // entt::sigh<void(ScanStorage)> entry_selected;
  };

  // Updated to accept the vector of storages provided by the Model
  void Render(const ScanStorage& entries);

  // clang-format off
  struct Sinks {
    ScannerWidget& view;
    auto NewScanPressed() {return entt::sink(view.signals_.new_scan_pressed);}
    // auto NewScanPressed() {return entt::sink(view.signals_.new_scan_pressed);}
    auto NextScanPressed() {return entt::sink(view.signals_.next_scan_pressed);}
    auto ScanComparisonSelected() {return entt::sink(view.signals_.scan_comparison_selected);}
  };

  // clang-format on

  Sinks sinks() {
    return Sinks{*this};
  }

 private:
  void EmitSetComparisonSelected() const;

  Signals signals_;

  std::string str_;
  int selected_index_ = 0;
  bool is_hex_input_ = false;
  int current_type_index_ = static_cast<int>(ScanValueType::kInt32);
  int selected_comparison_index_ = static_cast<int>(ScanComparison::kChanged);
};

}  // namespace maia
