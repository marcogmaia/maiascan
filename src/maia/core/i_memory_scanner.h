// Copyright (c) Maia

#pragma once

#include <span>
#include <vector>

namespace maia::core {

/// \brief Defines a contract for performing memory scanning operations
/// against a specific process.
class IMemoryScanner {
 public:
  virtual ~IMemoryScanner() = default;

  /// \brief Performs an initial scan of the process.
  /// \param value A span representing the value to search for.
  /// \return A std::vector of addresses where the value was found.
  [[nodiscard]] virtual std::vector<uintptr_t> FirstScan(
      std::span<const std::byte> value) = 0;

  // /// \brief Performs a subsequent scan for an exact value.
  // /// \param previous_results A span of addresses from the last scan.
  // /// \param new_value A span representing the new value to search for.
  // /// \return A std::vector of addresses that still match the new value.
  // [[nodiscard]] virtual std::vector<uintptr_t> NextScanExactValue(
  //     std::span<const uintptr_t> previous_results,
  //     std::span<const std::byte> new_value) = 0;

  // /// \brief Performs a scan for values that have not changed.
  // /// \param previous_results A span of addresses from the last scan.
  // /// \return A std::vector of addresses whose values are unchanged.
  // [[nodiscard]] virtual std::vector<uintptr_t> NextScanUnchangedValue(
  //     std::span<const uintptr_t> previous_results) = 0;
};

}  // namespace maia::core
