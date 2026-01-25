// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <entt/signal/sigh.hpp>
#include <string>
#include <vector>

#include "maia/core/address_formatter.h"
#include "maia/core/scan_types.h"

namespace maia {

class ScannerWidget {
 public:
  void Render(const ScanStorage& entries,
              const AddressFormatter& formatter,
              float progress,
              bool is_scanning);

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
    entt::sigh<void(std::vector<std::byte> /* value */)> target_value_selected;

    /// \brief Emitted when the user selects a different comparison type (e.g.,
    /// Exact Value, Changed).
    /// \param comparison The selected comparison mode.
    entt::sigh<void(ScanComparison /* comparison */)> scan_comparison_selected;

    /// \brief Emitted when the user toggles the "Auto Update" checkbox.
    /// \param enabled True if auto-update is enabled, false otherwise.
    entt::sigh<void(bool /* enabled */)> auto_update_changed;

    /// \brief Emitted when the user toggles the "Pause while scanning"
    /// checkbox.
    /// \param enabled True if enabled, false otherwise.
    entt::sigh<void(bool /* enabled */)> pause_while_scanning_changed;

    /// \brief Emitted when the user toggles the "Fast Scan" checkbox.
    /// \param enabled True if enabled, false otherwise.
    entt::sigh<void(bool /* enabled */)> fast_scan_changed;

    /// \brief Emitted when an entry in the results table is double-clicked.
    /// \param index The index of the clicked entry within the current scan
    /// storage.
    /// \param type The value type of the clicked entry.
    entt::sigh<void(int, ScanValueType)> entry_double_clicked;

    /// \brief Emitted when the user clicks the "Cancel" button.
    entt::sigh<void()> cancel_scan_pressed;
  };

  // clang-format off
  struct Sinks {
    ScannerWidget& view;
    auto NewScanPressed() {return entt::sink(view.signals_.new_scan_pressed);}
    auto NextScanPressed() {return entt::sink(view.signals_.next_scan_pressed);}
    auto TargetValueSelected() {return entt::sink(view.signals_.target_value_selected);}
    auto ScanComparisonSelected() {return entt::sink(view.signals_.scan_comparison_selected);}
    auto AutoUpdateChanged() {return entt::sink(view.signals_.auto_update_changed);}
    auto PauseWhileScanningChanged() {return entt::sink(view.signals_.pause_while_scanning_changed);}
    auto FastScanChanged() {return entt::sink(view.signals_.fast_scan_changed);}
    auto EntryDoubleClicked() {return entt::sink(view.signals_.entry_double_clicked);}
    auto CancelScanPressed() {return entt::sink(view.signals_.cancel_scan_pressed);}
  };

  // clang-format on

  void EmitSetComparisonSelected() const;

  Signals signals_;

  std::string str_;
  int selected_index_ = 0;
  bool is_hex_input_ = false;
  bool auto_update_enabled_ = false;
  bool pause_while_scanning_enabled_ = false;
  bool fast_scan_enabled_ = true;
  int current_type_index_ = static_cast<int>(ScanValueType::kInt32);
  int selected_comparison_index_ = static_cast<int>(ScanComparison::kChanged);
};

}  // namespace maia
