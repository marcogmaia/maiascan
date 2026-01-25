// Copyright (c) Maia

#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "maia/core/i_process.h"
#include "maia/core/scan_types.h"

namespace maia {

/// \brief Manages memory scanning logic, result storage, and background
/// updates.
class ScanResultModel {
 public:
  auto sinks() {
    return Sinks{*this};
  }

  const ScanStorage& entries() const {
    return scan_storage_;
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

  void SetPauseWhileScanning(bool enabled) {
    pause_while_scanning_enabled_ = enabled;
  }

  void SetFastScan(bool enabled) {
    fast_scan_enabled_ = enabled;
  }

  bool IsFastScanEnabled() const {
    return fast_scan_enabled_;
  }

  void CancelScan();

  bool IsScanning() const {
    return is_scanning_.load();
  }

  float GetProgress() const {
    return progress_.load();
  }

  bool HasPendingResult() const {
    return scan_finished_flag_.load();
  }

  void ApplyPendingResult();

  void Clear();

  void StartAutoUpdate();
  void StopAutoUpdate();

  void UpdateCurrentValues();

  std::vector<mmem::ModuleDescriptor> GetModules() const {
    std::scoped_lock lock(mutex_);
    return modules_;
  }

 private:
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

  void AutoUpdateLoop(std::stop_token stop_token);

  Signals signals_;
  IProcess* active_process_{};

  ScanComparison scan_comparison_{ScanComparison::kChanged};
  ScanValueType scan_value_type_{ScanValueType::kUInt32};
  std::vector<std::byte> target_scan_value_;
  bool pause_while_scanning_enabled_ = false;
  bool fast_scan_enabled_ = true;  // Default: aligned scanning for speed

  // Current list of matches.

  // Every time a successful scan is made (First or Next), it also updates the
  // "prev" with the most recent scan result.
  ScanStorage scan_storage_;

  // Async scanning state.
  std::atomic<bool> is_scanning_{false};
  std::atomic<float> progress_{0.0f};
  std::atomic<bool> scan_finished_flag_{false};
  std::jthread scan_thread_;

  // Storage for the result of the background scan, waiting to be swapped in.
  ScanStorage pending_storage_;
  mutable std::mutex pending_mutex_;

  mutable std::mutex mutex_;
  std::jthread task_;
  std::vector<mmem::ModuleDescriptor> modules_;
};

}  // namespace maia
