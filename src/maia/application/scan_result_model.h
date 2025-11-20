// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>

#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"
#include "maia/core/scan_types.h"

namespace maia {

struct ScanStorage {
  std::vector<MemoryAddress> addresses;
  std::vector<std::byte> raw_values_buffer;
  size_t stride;
};

/// \brief Manages memory scanning logic, result storage, and background
/// updates.
///
/// This class acts as the Model in the MVP pattern. It performs memory scans
/// (First/Next), holds the results, and notifies listeners via EnTT signals
/// when data changes.
class ScanResultModel {
 public:
  struct Signals {
    /// \brief Emitted when the scan results change (entries added/removed or
    /// values updated).
    entt::sigh<void(const std::vector<ScanStorage>&)> memory_changed;
  };

  ScanResultModel();

  // ~ScanResultModel();

  /// \brief Access the signal registry for this model.
  Signals& signals() {
    return signals_;
  }

  /// \brief Read-only access to current scan entries.
  /// \warning Not thread-safe if AutoUpdate is running. The model should be
  /// locked or the update thread is paused before iterating this vector.
  const std::vector<ScanStorage>& entries() const {
    return curr_entries_;
  }

  /// \brief Performs the initial scan over the memory range of the active
  /// process.
  ///
  /// Populates the entries list with all addresses matching the current
  /// scan value type and logic.
  void FirstScan();

  /// \brief Filters the existing scan results based on the comparison logic.
  ///
  /// Iterates over the current entries, re-reads the memory, and keeps only
  /// those that satisfy the `ScanComparison` (e.g., value increased, changed).
  void NextScan();

  /// \brief Sets the process to be scanned.
  /// \param process Pointer to the process interface. The caller retains
  /// ownership.
  void SetActiveProcess(IProcess* process);

  /// \brief Sets the comparison method for the NextScan (e.g., ExactValue,
  /// Increased).
  void SetScanComparison(ScanComparison scan_comparison) {
    scan_comparison_ = scan_comparison;
  }

  /// \brief Sets the specific value to look for (used for ExactValue searches).
  void SetTargetScanValue(std::vector<std::byte> target_scan_value) {
    target_scan_value_ = std::move(target_scan_value);
  }

  /// \brief Clears all scan results and resets internal state.
  void Clear();

  /// \brief Starts a background thread that refreshes values of found
  /// addresses.
  ///
  /// The thread typically runs at 1Hz (every second).
  void StartAutoUpdate();

  /// \brief Stops the background update thread.
  void StopAutoUpdate();

 private:
  /// \brief Helper to refresh the data of current entries without filtering.
  ///
  /// Reads memory for all addresses in `entries_` and updates the `data` field.
  /// Triggers the `memory_changed` signal.
  void UpdateCurrentValues();

  /// \brief The loop function executed by the jthread.
  void AutoUpdateLoop(std::stop_token stop_token);

  Signals signals_;

  // Non-owning pointer to the target process.
  IProcess* active_process_{};

  ScanComparison scan_comparison_{ScanComparison::kChanged};
  ScanValueType scan_value_type_{ScanValueType::kUInt32};
  std::vector<std::byte> target_scan_value_;

  // Current list of matches.
  std::vector<ScanStorage> curr_entries_;

  // Comparison snapshot for "Changed", "Increased", "Decreased" logic.
  std::vector<ScanStorage> prev_entries_;

  // Protects access to entries_ during background updates.
  mutable std::mutex mutex_;

  // This joins automatically on destruction.
  std::jthread task_;
};

}  // namespace maia
