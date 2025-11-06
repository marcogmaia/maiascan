// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"

namespace maia {

// enum class ScanValueType {};

/// Defines the type of comparison to perform during a scan.
enum class ScanType {
  kExactValue,
  kBiggerThan,
  kSmallerThan,
  // kCount  // Helper for array sizes
};

/// Defines the data type to scan for.
enum class ValueType {
  kFourBytes,
  kOneByte,
  kTwoBytes,
  kEightBytes,
  kFloat,
  kDouble,
  // kCount  // Helper for array sizes
};

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
  int current_scan_type_{};
  int current_scan_value_type_{};
};

}  // namespace maia
