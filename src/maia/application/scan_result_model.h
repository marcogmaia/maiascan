// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

#include <entt/signal/sigh.hpp>

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
class ScanResultModel {
 public:
  struct Signals {
    /// \brief Emitted when the scan results change.
    entt::sigh<void(const ScanStorage&)> memory_changed;
  };

  // clang-format off
  struct Sinks { 
    ScanResultModel& model;
    auto MemoryChanged() { return entt::sink(model.signals_.memory_changed); };
  };

  // clang-format on

  ScanResultModel();

  Sinks sinks() {
    return Sinks{*this};
  }

  const ScanStorage& entries() const {
    return curr_entries_;
  }

  void FirstScan();
  void NextScan();

  void SetActiveProcess(IProcess* process);

  void SetScanComparison(ScanComparison scan_comparison) {
    scan_comparison_ = scan_comparison;
  }

  void SetTargetScanValue(std::vector<std::byte> target_scan_value) {
    target_scan_value_ = std::move(target_scan_value);
  }

  void Clear();

  void StartAutoUpdate();
  void StopAutoUpdate();

 private:
  // friend Sinks;
  void UpdateCurrentValues();
  void AutoUpdateLoop(std::stop_token stop_token);

  Signals signals_;
  IProcess* active_process_{};

  ScanComparison scan_comparison_{ScanComparison::kChanged};
  ScanValueType scan_value_type_{ScanValueType::kUInt32};
  std::vector<std::byte> target_scan_value_;

  // Current list of matches (SoA).
  ScanStorage curr_entries_;

  // Snapshot for comparison (SoA).
  ScanStorage prev_entries_;

  mutable std::mutex mutex_;
  std::jthread task_;
};

}  // namespace maia
