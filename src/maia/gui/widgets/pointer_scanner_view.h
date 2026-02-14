// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <chrono>
#include <cstdint>
#include <entt/signal/sigh.hpp>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "maia/application/cheat_table_model.h"
#include "maia/core/pointer_scanner.h"
#include "maia/core/scan_types.h"

namespace maia {

/// \brief ImGui view for the pointer scanner interface.
/// \details Provides controls for generating pointer maps, configuring scans,
/// and displaying pointer path results.
class PointerScannerView {
 public:
  PointerScannerView();

  /// \brief Path resolver function type.
  using PathResolver =
      std::function<std::optional<uint64_t>(const core::PointerPath&)>;

  /// \brief Value reader function type for reading values at addresses.
  using ValueReader =
      std::function<std::optional<std::vector<std::byte>>(uint64_t address)>;

  /// \brief Render the pointer scanner window.
  /// \param is_open Pointer to control window visibility (ImGui pattern).
  /// \param paths The discovered pointer paths.
  /// \param map_entry_count Number of entries in the pointer map.
  /// \param map_progress Progress of map generation (0.0-1.0).
  /// \param scan_progress Progress of path scan (0.0-1.0).
  /// \param is_generating_map Whether map generation is in progress.
  /// \param is_scanning Whether path scanning is in progress.
  /// \param cheat_entries Available cheat table entries for target selection.
  /// \param scan_results Available scan results for target selection.
  /// \param available_modules List of loaded modules for filtering.
  /// \param path_resolver Optional callback to resolve a path's current
  /// address.
  /// \param value_reader Optional callback to read values at addresses.
  /// \param value_type The type of values to display.
  void Render(bool* is_open,
              const std::vector<core::PointerPath>& paths,
              size_t map_entry_count,
              float map_progress,
              float scan_progress,
              bool is_generating_map,
              bool is_scanning,
              const std::vector<CheatTableEntry>& cheat_entries,
              const ScanStorage& scan_results,
              const std::vector<std::string>& available_modules,
              PathResolver path_resolver = nullptr,
              ValueReader value_reader = nullptr,
              ScanValueType value_type = ScanValueType::kUInt32);

  auto sinks() {
    return Sinks{*this};
  }

  /// \brief Get the current scan configuration from UI state.
  [[nodiscard]] core::PointerScanConfig GetScanConfig() const;

  /// \brief Update the selected target type in the UI (e.g. when selecting from
  /// other sources).
  void SetSelectedType(ScanValueType type) {
    selected_type_ = type;
  }

  /// \brief Set the target address in the UI (e.g., when resolved from cheat
  /// table).
  /// \param address The address to display (will be formatted as hex).
  void SetTargetAddress(uint64_t address);

  /// \brief Tracks state for visible rows in the results table.
  struct RowState {
    std::string last_value;
    std::chrono::steady_clock::time_point last_change;
  };

  static constexpr size_t kDefaultMaxDisplayedResults = 100;

 private:
  class Signals {
   public:
    /// \brief Target address changed via manual input.
    entt::sigh<void(uint64_t /* address */)> target_address_changed;

    /// \brief Target address validation failed.
    entt::sigh<void()> target_address_invalid;

    /// \brief Target type changed.
    entt::sigh<void(ScanValueType /* type */)> target_type_changed;

    /// \brief Target address selected from cheat table.
    entt::sigh<void(size_t /* index */)> target_from_cheat_selected;

    /// \brief Target address selected from scan results.
    entt::sigh<void(size_t /* index */)> target_from_scan_selected;

    /// \brief Generate pointer map button pressed.
    entt::sigh<void()> generate_map_pressed;

    /// \brief Save map button pressed.
    entt::sigh<void()> save_map_pressed;

    /// \brief Load map button pressed.
    entt::sigh<void()> load_map_pressed;

    /// \brief Find paths button pressed.
    entt::sigh<void()> find_paths_pressed;

    /// \brief Validate paths button pressed.
    entt::sigh<void()> validate_pressed;

    /// \brief Cancel operation button pressed.
    entt::sigh<void()> cancel_pressed;

    /// \brief Pointer path result double-clicked.
    entt::sigh<void(size_t /* index */)> result_double_clicked;

    /// \brief Show all results button pressed.
    entt::sigh<void()> show_all_pressed;
  };

  struct Sinks {
    PointerScannerView& view;

    // clang-format off
    auto TargetAddressChanged() { return entt::sink(view.signals_.target_address_changed); }
    auto TargetAddressInvalid() { return entt::sink(view.signals_.target_address_invalid); }
    auto TargetTypeChanged() { return entt::sink(view.signals_.target_type_changed); }
    auto TargetFromCheatSelected() { return entt::sink(view.signals_.target_from_cheat_selected); }
    auto TargetFromScanSelected() { return entt::sink(view.signals_.target_from_scan_selected); }
    auto GenerateMapPressed() { return entt::sink(view.signals_.generate_map_pressed); }
    auto SaveMapPressed() { return entt::sink(view.signals_.save_map_pressed); }
    auto LoadMapPressed() { return entt::sink(view.signals_.load_map_pressed); }
    auto FindPathsPressed() { return entt::sink(view.signals_.find_paths_pressed); }
    auto ValidatePressed() { return entt::sink(view.signals_.validate_pressed); }
    auto CancelPressed() { return entt::sink(view.signals_.cancel_pressed); }
    auto ResultDoubleClicked() { return entt::sink(view.signals_.result_double_clicked); }
    auto ShowAllPressed() { return entt::sink(view.signals_.show_all_pressed); }

    // clang-format on
  };

  void RenderTargetSection(const std::vector<CheatTableEntry>& cheat_entries,
                           const ScanStorage& scan_results);

  void RenderTargetAddressInput();
  void RenderTypeSelector();
  void RenderSourceSelector(const std::vector<CheatTableEntry>& cheat_entries,
                            const ScanStorage& scan_results);

  void RenderMapSection(size_t map_entry_count,
                        float map_progress,
                        bool is_generating_map) const;
  void RenderConfigSection(const std::vector<std::string>& available_modules);
  void RenderActionSection(bool is_generating_map,
                           bool is_scanning,
                           bool has_paths,
                           float scan_progress) const;
  void RenderResultsSection(const std::vector<core::PointerPath>& paths,
                            bool is_scanning,
                            PathResolver path_resolver,
                            ValueReader value_reader,
                            ScanValueType value_type);
  void RenderResultsStatus(const std::vector<core::PointerPath>& paths,
                           bool is_scanning);

  Signals signals_;

  // UI State
  std::string target_address_str_;
  bool target_address_valid_ = true;
  ScanValueType selected_type_ = ScanValueType::kUInt32;
  int selected_source_ = 0;  // 0=manual, 1=cheat, 2=scan
  int selected_cheat_index_ = -1;
  int selected_scan_index_ = -1;

  // Configuration
  int max_level_ = 7;
  int max_offset_ = 4096;
  bool allow_negative_offsets_ = false;
  int max_results_ = 0;  // 0 = unlimited
  std::vector<std::string> allowed_modules_;
  std::string module_filter_input_;
  std::string last_offsets_input_;  // User input for known last offsets

  // Display
  bool show_all_results_ = false;

  // Row state tracking for value change blinking (keyed by path, not index)
  std::unordered_map<std::string, RowState> visible_row_states_;
  const void* last_paths_ptr_ = nullptr;  // To detect when paths vector changes
};

}  // namespace maia
