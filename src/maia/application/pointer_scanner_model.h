// Copyright (c) Maia

/// \file pointer_scanner_model.h
/// \brief Manages the multi-stage pointer scanning workflow.
///
/// \details
/// **Role**: Controls the complex workflow of finding static pointer paths to a
/// dynamic address.
///
/// **Workflow**:
///    1. **Pointer Map Generation**: Snapshots the entire process memory
///    layout.
///    2. **Path Finding**: Searches the graph for paths from static bases to
///    the target.
///    3. **Validation**: verifies paths against current process state.
///
/// **Architecture**:
///    - **Async State Machine**: Tracks multiple states (Generating, Scanning,
///    Validating).
///    - **Heavy Computation**: Most operations are offloaded to `std::async` or
///    `std::jthread`.
///
/// **Thread Safety**:
///    - Uses atomic progress indicators and mutex-protected result storage.
///
/// **Key Interactions**:
///    - Uses `core::PointerScanner` and `core::PointerMap`.
///    - Consumed by `PointerScannerPresenter`.

#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "maia/core/i_process.h"
#include "maia/core/pointer_map.h"
#include "maia/core/pointer_scanner.h"
#include "maia/core/scan_types.h"

namespace maia {

/// \brief Current state of the pointer scanner.
enum class ScannerState {
  kIdle,
  kGeneratingMap,
  kScanning,
  kValidating,
  kCancelling
};

/// \brief Manages pointer scanning operations and state.
/// \details Coordinates pointer map generation, pointer path discovery,
/// and result management for the pointer scanner UI.
class PointerScannerModel {
 public:
  PointerScannerModel();
  ~PointerScannerModel();

  auto sinks() {
    return Sinks{*this};
  }

  /// \brief Set the target address manually (hex input).
  void SetTargetAddress(uint64_t address);

  /// \brief Get the current target address.
  [[nodiscard]] uint64_t GetTargetAddress() const {
    return target_address_.load();
  }

  /// \brief Set the target value type.
  void SetTargetType(ScanValueType type);

  /// \brief Get the current target value type.
  [[nodiscard]] ScanValueType GetTargetType() const {
    return target_type_.load();
  }

  /// \brief Set the active process for scanning.
  /// \details If an operation is in progress, it will be cancelled and waited
  ///          for before switching. This ensures no dangling pointers.
  void SetActiveProcess(IProcess* process);

  /// \brief Generate a pointer map from the active process (async).
  void GeneratePointerMap();

  /// \brief Save the current pointer map to disk.
  [[nodiscard]] bool SaveMap(const std::filesystem::path& path) const;

  /// \brief Load a pointer map from disk.
  [[nodiscard]] bool LoadMap(const std::filesystem::path& path);

  /// \brief Find pointer paths to the target address (async).
  /// \param config The scan configuration.
  void FindPaths(const core::PointerScanConfig& config);

  /// \brief Validate existing paths against current process state (sync).
  /// \return List of paths that still resolve to the target.
  [[nodiscard]] std::vector<core::PointerPath> ValidatePaths() const;

  /// \brief Validate existing paths asynchronously (does not block UI).
  /// Results are emitted via the validation_complete signal.
  void ValidatePathsAsync();

  /// \brief Set the pointer paths (used after validation to update results).
  /// \param paths The new list of pointer paths.
  void SetPaths(const std::vector<core::PointerPath>& paths);

  /// \brief Cancel any ongoing operation.
  void CancelOperation();

  /// \brief Check if a pointer map exists.
  [[nodiscard]] bool HasPointerMap() const {
    std::scoped_lock lock(mutex_);
    return pointer_map_.has_value();
  }

  /// \brief Get the number of entries in the pointer map.
  [[nodiscard]] size_t GetMapEntryCount() const {
    std::scoped_lock lock(mutex_);
    return pointer_map_ ? pointer_map_->GetEntryCount() : 0;
  }

  /// \brief Get the discovered pointer paths.
  [[nodiscard]] std::vector<core::PointerPath> GetPaths() const {
    std::scoped_lock lock(mutex_);
    return paths_;
  }

  /// \brief Check if currently generating a map.
  [[nodiscard]] bool IsGeneratingMap() const {
    return state_.load() == ScannerState::kGeneratingMap;
  }

  /// \brief Check if currently scanning for paths.
  [[nodiscard]] bool IsScanning() const {
    return state_.load() == ScannerState::kScanning;
  }

  /// \brief Check if currently validating paths.
  [[nodiscard]] bool IsValidating() const {
    return state_.load() == ScannerState::kValidating;
  }

  /// \brief Check if any operation is in progress.
  [[nodiscard]] bool IsBusy() const {
    return state_.load() != ScannerState::kIdle;
  }

  /// \brief Check if an operation is currently being cancelled.
  [[nodiscard]] bool IsCancelling() const {
    return state_.load() == ScannerState::kCancelling;
  }

  /// \brief Get current operation progress (0.0 to 1.0).
  [[nodiscard]] float GetProgress() const {
    return progress_.load();
  }

  /// \brief Get pointer map generation progress (0.0 to 1.0).
  [[nodiscard]] float GetMapProgress() const {
    return map_progress_.load();
  }

  /// \brief Get pointer scan progress (0.0 to 1.0).
  [[nodiscard]] float GetScanProgress() const {
    return scan_progress_.load();
  }

  /// \brief Get the current operation name.
  [[nodiscard]] std::string GetCurrentOperation() const {
    std::scoped_lock lock(mutex_);
    return current_operation_;
  }

  /// \brief Check if there's a pending scan result to apply.
  [[nodiscard]] bool HasPendingResult() const;

  /// \brief Apply the pending scan result (call from main thread).
  void ApplyPendingResult();

  /// \brief Wait for any ongoing operation to complete.
  void WaitForOperation();

  /// \brief Clear all paths and results.
  void Clear();

  /// \brief Get the list of available module names from the active process.
  [[nodiscard]] std::vector<std::string> GetModuleNames() const;

  /// \brief Resolve a single path using the active process.
  /// \param path The pointer path to resolve.
  /// \return The resolved address or nullopt.
  [[nodiscard]] std::optional<uint64_t> ResolvePath(
      const core::PointerPath& path) const;

  /// \brief Update the model (apply pending results).
  /// \details This should be called once per frame from the main thread.
  void Update();

 private:
  friend struct MapResultHandler;
  friend struct ScanResultHandler;
  friend struct ValidationResultHandler;

  struct Signals {
    /// \brief Emitted when pointer map generation completes.
    /// \param success Whether generation succeeded.
    /// \param entry_count Number of entries in the map.
    entt::sigh<void(bool /* success */, size_t /* entry_count */)>
        map_generated;

    /// \brief Emitted when a scan completes.
    /// \param result The scan result containing paths.
    entt::sigh<void(const core::PointerScanResult& /* result */)> scan_complete;

    /// \brief Emitted when progress updates.
    /// \param progress Progress value 0.0-1.0.
    /// \param operation Name of current operation.
    entt::sigh<void(float /* progress */, const std::string& /* operation */)>
        progress_updated;

    /// \brief Emitted when paths are updated (cleared, validated, etc.).
    entt::sigh<void()> paths_updated;

    /// \brief Emitted when async validation completes.
    /// \param valid_paths The list of paths that still resolve to target.
    entt::sigh<void(const std::vector<core::PointerPath>& /* valid_paths */)>
        validation_complete;
  };

  struct Sinks {
    PointerScannerModel& model;

    auto MapGenerated() {
      return entt::sink(model.signals_.map_generated);
    }

    auto ScanComplete() {
      return entt::sink(model.signals_.scan_complete);
    }

    auto ProgressUpdated() {
      return entt::sink(model.signals_.progress_updated);
    }

    auto PathsUpdated() {
      return entt::sink(model.signals_.paths_updated);
    }

    auto ValidationComplete() {
      return entt::sink(model.signals_.validation_complete);
    }
  };

  void OnProgressUpdated(float progress, const std::string& operation);

  Signals signals_;

  // Target address (atomic for thread-safe reads)
  std::atomic<uint64_t> target_address_{0};

  // Target value type (atomic for thread-safe reads)
  std::atomic<ScanValueType> target_type_{ScanValueType::kUInt32};

  // Process reference
  IProcess* active_process_{nullptr};
  std::vector<mmem::ModuleDescriptor> modules_;

  // Pointer map storage
  std::optional<core::PointerMap> pointer_map_;

  // Scan results
  std::vector<core::PointerPath> paths_;

  // Async operation state
  std::atomic<ScannerState> state_{ScannerState::kIdle};
  std::atomic<bool> cancelled_{false};  // Set to abort operations
  std::atomic<float> progress_{0.0f};
  std::atomic<float> map_progress_{0.0f};
  std::atomic<float> scan_progress_{0.0f};
  std::string current_operation_;

  std::future<std::optional<core::PointerMap>> pending_map_;
  std::future<core::PointerScanResult> pending_scan_;
  std::future<std::vector<core::PointerPath>> pending_validation_;
  std::stop_source stop_source_;

  mutable std::mutex mutex_;
};

}  // namespace maia
