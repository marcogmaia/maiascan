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
    /// \brief Emitted when the user clicks the "First Scan" button.
    entt::sigh<void()> new_scan_pressed;

    /// \brief Emitted when the user clicks the "Next Scan" button.
    entt::sigh<void()> next_scan_pressed;

    /// \brief Emitted when the user changes the target scan value in the input
    /// field.
    /// \param value The parsed byte representation of the new target value.
    entt::sigh<void(std::vector<std::byte>)> target_value_selected;

    /// \brief Emitted when the user selects a different comparison type (e.g.,
    /// Exact Value, Changed).
    /// \param comparison The selected comparison mode.
    entt::sigh<void(ScanComparison)> scan_comparison_selected;

    /// \brief Emitted when the user toggles the "Auto Update" checkbox.
    /// \param enabled True if auto-update is enabled, false otherwise.
    entt::sigh<void(bool)> auto_update_changed;

    /// \brief Emitted when an entry in the results table is double-clicked.
    /// \param index The index of the clicked entry within the current scan
    /// storage.
    /// \param type The value type of the clicked entry.
    entt::sigh<void(int, ScanValueType)> entry_double_clicked;
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
