// Copyright (c) Maia

/// \file scanner.h
/// \brief Stateless memory scanning engine.
///
/// \details
/// **Role**: The core computational engine that searches memory. It takes a
/// process and a configuration, performs the search, and returns results.
///
/// **Architecture**:
///    - **Stateless Service**: Does not retain state between calls. Each
///    `FirstScan`
///      or `NextScan` is an independent operation.
///    - **Functional**: Designed to be easily wrapped in async tasks or used
///    directly.
///
/// **Thread Safety**:
///    - The class itself is immutable and thread-safe.
///    - Can be instantiated on any thread.
///
/// **Key Interactions**:
///    - Orchestrated by `ScanResultModel`.
///    - Uses `IProcess` to read memory.
///    - Uses `SimdScanner` (internal) for optimized SIMD pattern matching.

#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <stop_token>

#include "maia/core/i_process.h"
#include "maia/core/scan_config.h"
#include "maia/core/scan_types.h"

namespace maia::core {

/// \brief Result of a scan operation.
struct ScanResult {
  ScanStorage storage;
  bool success = false;
  std::string error_message;
};

/// \brief Progress callback signature.
/// \param progress Value between 0.0 and 1.0 indicating completion percentage.
using ProgressCallback = std::function<void(float progress)>;

/// \brief Stateless memory scanner service.
/// \details Performs memory scanning operations given an IProcess and
/// ScanConfig. This class is designed to be used by both GUI and CLI without
/// any UI dependencies. Each Scan() call is independent and does not retain
/// state between calls.
class Scanner {
 public:
  Scanner() = default;
  ~Scanner() = default;

  Scanner(const Scanner&) = delete;
  Scanner& operator=(const Scanner&) = delete;
  Scanner(Scanner&&) = default;
  Scanner& operator=(Scanner&&) = default;

  /// \brief Performs a first scan (searches all memory regions).
  /// \param process The target process to scan.
  /// \param config The scan configuration.
  /// \param stop_token Token to request cancellation.
  /// \param progress_callback Optional callback for progress updates.
  /// \return The scan results.
  [[nodiscard]] ScanResult FirstScan(
      IProcess& process,
      const ScanConfig& config,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr) const;

  /// \brief Performs a next scan (filters existing results).
  /// \param process The target process to scan.
  /// \param config The scan configuration.
  /// \param previous_results The results from the previous scan to filter.
  /// \param stop_token Token to request cancellation.
  /// \param progress_callback Optional callback for progress updates.
  /// \return The filtered scan results.
  [[nodiscard]] ScanResult NextScan(
      IProcess& process,
      const ScanConfig& config,
      const ScanStorage& previous_results,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr) const;

  /// \brief Async version of FirstScan.
  /// \return A future that will contain the scan results.
  [[nodiscard]] std::future<ScanResult> FirstScanAsync(
      IProcess& process,
      const ScanConfig& config,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr) const;

  /// \brief Async version of NextScan.
  /// \return A future that will contain the scan results.
  [[nodiscard]] std::future<ScanResult> NextScanAsync(
      IProcess& process,
      const ScanConfig& config,
      const ScanStorage& previous_results,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr) const;

  /// \brief Sets the chunk size for memory reading.
  /// \param size The chunk size in bytes (default: 32MB).
  void SetChunkSize(size_t size) {
    chunk_size_ = size;
  }

  /// \brief Gets the current chunk size.
  [[nodiscard]] size_t GetChunkSize() const {
    return chunk_size_;
  }

 private:
  static constexpr size_t k32Mb = 32z * 1 << 20;
  size_t chunk_size_ = k32Mb;
};

}  // namespace maia::core
