// Copyright (c) Maia

/// \file scan_result_model.h
/// \brief Manages the state and execution of memory scans.
///
/// \details
/// **Role**: The high-level controller for the scanning feature. It maintains
/// the current list of found addresses (`ScanSession`) and orchestrates the
/// background scanning tasks.
///
/// **Architecture**:
///    - **Stateful Async Controller**: Manages `std::future` for long-running
///    scans
///      to keep the UI responsive.
///    - **Background Worker**: Handles the "Auto-Update" loop to refresh values
///    of found results.
///
/// **Thread Safety**:
///    - High. Uses internal mutexes to protect result storage.
///    - Uses `std::stop_source` for cancellable async tasks.
///
/// **Key Interactions**:
///    - Uses `core::Scanner` to perform actual work.
///    - Listens to `ProcessModel` to clear results on process switch.
///    - Consumed by `ScannerPresenter` for UI visualization.

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
  /// \brief Constructs the model with a specific chunk size for memory reading.
  explicit ScanResultModel(size_t chunk_size = 32 * 1024 * 1024);
  ~ScanResultModel();

  /// \brief Exposes signal sinks for EnTT connections.
  auto sinks() {
    return Sinks{*this};
  }

  /// \brief Returns the current storage of found addresses.
  /// \note This is thread-safe to read, but the returned reference lifetime
  /// is bound to the model's internal session.
  const ScanStorage& entries() const;

  /// \brief Initiates a new scan (First Scan) on the active process.
  /// \details Clears previous results and starts an async task.
  /// Does nothing if a scan is already in progress or process is invalid.
  void FirstScan();

  /// \brief Initiates a filter scan (Next Scan) on existing results.
  /// \details Filters the current list based on the new criteria.
  /// Starts an async task. Does nothing if no previous results exist.
  void NextScan();

  /// \brief Sets the active process to scan.
  /// \details Updates internal module list and clears existing results.
  void SetActiveProcess(IProcess* process);

  /// \brief Sets the comparison mode (e.g., Exact, GreaterThan, Changed).
  void SetScanComparison(ScanComparison scan_comparison) {
    scan_comparison_ = scan_comparison;
  }

  /// \brief Sets the value type to scan for (e.g., Int32, Float).
  void SetScanValueType(ScanValueType scan_value_type) {
    scan_value_type_ = scan_value_type;
  }

  /// \brief Sets the target value for Exact match scans.
  void SetTargetScanValue(std::vector<std::byte> target_scan_value) {
    target_scan_value_ = std::move(target_scan_value);
    target_scan_mask_.clear();
  }

  /// \brief Sets the target value and mask for Pattern scans.
  void SetTargetScanPattern(std::vector<std::byte> value,
                            std::vector<std::byte> mask) {
    target_scan_value_ = std::move(value);
    target_scan_mask_ = std::move(mask);
  }

  /// \brief Configures whether the game should be paused during the scan.
  void SetPauseWhileScanning(bool enabled) {
    pause_while_scanning_enabled_ = enabled;
  }

  /// \brief Configures whether to use alignment optimizations (Fast Scan).
  void SetFastScan(bool enabled) {
    fast_scan_enabled_ = enabled;
  }

  /// \brief Checks if Fast Scan is enabled.
  bool IsFastScanEnabled() const {
    return fast_scan_enabled_;
  }

  /// \brief Requests cancellation of the current async scan.
  void CancelScan();

  /// \brief Checks if a scan operation is currently running.
  bool IsScanning() const {
    return is_scanning_.load();
  }

  /// \brief Gets the progress of the current scan (0.0 to 1.0).
  float GetProgress() const {
    return progress_.load();
  }

  /// \brief Checks if the async scan has completed and results are ready.
  bool HasPendingResult() const;

  /// \brief Blocks the calling thread until the scan completes.
  void WaitForScanToFinish();

  /// \brief Applies the results of a finished scan to the main session.
  /// \details Should be called from the main thread when HasPendingResult() is
  /// true. This triggers the MemoryChanged signal.
  void ApplyPendingResult();

  /// \brief Clears all scan results.
  void Clear();

  /// \brief Starts the background thread that refreshes values.
  void StartAutoUpdate();

  /// \brief Stops the background auto-update thread.
  void StopAutoUpdate();

  /// \brief Manually triggers a value refresh for current results.
  void UpdateCurrentValues();

  /// \brief Reinterprets existing scan results as a different data type.
  void ChangeResultType(ScanValueType new_type);

  /// \brief Returns a copy of the cached module list.
  std::vector<mmem::ModuleDescriptor> GetModules() const {
    std::scoped_lock lock(mutex_);
    return modules_;
  }

  /// \brief Returns the configuration used for the current session.
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
