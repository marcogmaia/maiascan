// Copyright (c) Maia

#pragma once

#include <shared_mutex>
#include <vector>

#include "maia/core/scan_config.h"
#include "maia/core/scan_types.h"

namespace maia::core {

/// \brief Manages the state of a memory scanning session.
/// \details The Session owns the scan results and the configuration that
/// produced them. It provides thread-safe access for UI rendering (read) and
/// scanner updates (write). This separation enables features like Undo/Redo
/// and Save/Load in the future.
class ScanSession {
 public:
  ScanSession() = default;
  ~ScanSession() = default;

  ScanSession(const ScanSession&) = delete;
  ScanSession& operator=(const ScanSession&) = delete;
  ScanSession(ScanSession&&) = delete;
  ScanSession& operator=(ScanSession&&) = delete;

  /// \brief Returns a snapshot of the current scan storage.
  /// \details This is a copy, safe to use from the UI thread without holding
  /// a lock for extended periods.
  [[nodiscard]] ScanStorage GetStorageSnapshot() const {
    std::shared_lock lock(mutex_);
    return storage_;
  }

  /// \brief Returns a const reference to the storage (requires external sync).
  /// \details Use this only when you need to avoid a copy and can guarantee
  /// the session won't be modified concurrently.
  [[nodiscard]] const ScanStorage& GetStorageUnsafe() const {
    return storage_;
  }

  /// \brief Returns the configuration used for the current/last scan.
  [[nodiscard]] ScanConfig GetConfig() const {
    std::shared_lock lock(mutex_);
    return config_;
  }

  /// \brief Commits new scan results to the session.
  /// \details Called by the Scanner after a scan completes. This atomically
  /// replaces the current storage with the new results.
  void CommitResults(ScanStorage new_storage, ScanConfig config) {
    std::unique_lock lock(mutex_);
    storage_ = std::move(new_storage);
    config_ = std::move(config);
  }

  /// \brief Updates the current values in the storage.
  /// \details Used for live-updating the displayed values without changing
  /// the address list.
  void UpdateCurrentValues(std::vector<std::byte> new_current) {
    std::unique_lock lock(mutex_);
    storage_.curr_raw = std::move(new_current);
  }

  /// \brief Clears all scan results.
  void Clear() {
    std::unique_lock lock(mutex_);
    storage_.addresses.clear();
    storage_.curr_raw.clear();
    storage_.prev_raw.clear();
    storage_.stride = 0;
  }

  /// \brief Reinterprets current results with a new data type.
  /// \details This updates the storage metadata. Values must be refreshed
  /// separately.
  void ChangeType(ScanValueType new_type, size_t new_stride) {
    std::unique_lock lock(mutex_);
    storage_.value_type = new_type;
    storage_.stride = new_stride;

    // Ensure buffers are the correct size for the new type to avoid OOB
    // access in the scanner.
    const size_t new_size = storage_.addresses.size() * new_stride;
    storage_.curr_raw.assign(new_size, std::byte{0});
    storage_.prev_raw.assign(new_size, std::byte{0});

    config_.value_type = new_type;
  }

  /// \brief Overwrites previous values with current values.
  /// \details Resets the baseline for relative comparisons (Changed/Unchanged).
  void ResetPreviousToCurrent() {
    std::unique_lock lock(mutex_);
    storage_.prev_raw = storage_.curr_raw;
  }

  /// \brief Returns the number of results in the session.
  [[nodiscard]] size_t GetResultCount() const {
    std::shared_lock lock(mutex_);
    return storage_.addresses.size();
  }

  /// \brief Checks if the session has any results.
  [[nodiscard]] bool HasResults() const {
    std::shared_lock lock(mutex_);
    return !storage_.addresses.empty();
  }

 private:
  mutable std::shared_mutex mutex_;
  ScanStorage storage_;
  ScanConfig config_;
};

}  // namespace maia::core
