// Copyright (c) Maia

#pragma once

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "maia/core/i_process.h"
#include "maia/core/scan_config.h"
#include "maia/core/scan_session.h"
#include "maia/core/scan_types.h"
#include "maia/core/scanner.h"

namespace maia {

/// \brief Manages memory scanning logic, result storage, and background
/// updates.
class ScanResultModel {
 public:
  explicit ScanResultModel(size_t chunk_size = 32 * 1024 * 1024);
  ~ScanResultModel();

  auto sinks() {
    return Sinks{*this};
  }

  const ScanStorage& entries() const;

  void FirstScan();
  void NextScan();

  void SetActiveProcess(IProcess* process);

  void SetScanComparison(ScanComparison scan_comparison) {
    scan_comparison_ = scan_comparison;
  }

  void SetScanValueType(ScanValueType scan_value_type) {
    scan_value_type_ = scan_value_type;
  }

  void SetTargetScanValue(std::vector<std::byte> target_scan_value) {
    target_scan_value_ = std::move(target_scan_value);
    target_scan_mask_.clear();
  }

  void SetTargetScanPattern(std::vector<std::byte> value,
                            std::vector<std::byte> mask) {
    target_scan_value_ = std::move(value);
    target_scan_mask_ = std::move(mask);
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

  bool HasPendingResult() const;

  void WaitForScanToFinish();

  void ApplyPendingResult();

  void Clear();

  void StartAutoUpdate();
  void StopAutoUpdate();

  void UpdateCurrentValues();

  std::vector<mmem::ModuleDescriptor> GetModules() const {
    std::scoped_lock lock(mutex_);
    return modules_;
  }

  core::ScanConfig GetSessionConfig() const {
    return session_->GetConfig();
  }

 private:
  struct Signals {
    entt::sigh<void(const ScanStorage&)> memory_changed;
  };

  // clang-format off
  struct Sinks { 
    ScanResultModel& model;
    auto MemoryChanged() { return entt::sink(model.signals_.memory_changed); };
  };

  // clang-format on

  core::ScanConfig BuildScanConfig(bool use_previous) const;

  void AutoUpdateLoop(std::stop_token stop_token);

  Signals signals_;
  IProcess* active_process_{};

  ScanComparison scan_comparison_{ScanComparison::kChanged};
  ScanValueType scan_value_type_{ScanValueType::kUInt32};
  std::vector<std::byte> target_scan_value_;
  std::vector<std::byte> target_scan_mask_;
  bool pause_while_scanning_enabled_ = false;
  bool fast_scan_enabled_ = true;

  // Core components
  std::shared_ptr<core::ScanSession> session_;
  core::Scanner scanner_;

  // Async scanning state
  std::atomic<bool> is_scanning_{false};
  std::atomic<float> progress_{0.0f};
  std::future<core::ScanResult> pending_scan_;
  std::stop_source stop_source_;
  core::ScanConfig pending_config_;

  mutable std::mutex mutex_;
  std::jthread task_;
  std::vector<mmem::ModuleDescriptor> modules_;
};

}  // namespace maia
