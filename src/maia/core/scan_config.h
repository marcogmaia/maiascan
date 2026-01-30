// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <vector>

#include "maia/core/scan_types.h"

namespace maia::core {

/// \brief Immutable configuration for a memory scan operation.
/// \details Consolidates all inputs required to perform a scan into a single
/// struct. This enables clean APIs where the caller builds a config and passes
/// it to the Scanner in one call, rather than setting state bit-by-bit.
struct ScanConfig {
  /// \brief The data type being scanned (e.g., Int32, Float).
  ScanValueType value_type = ScanValueType::kUInt32;

  /// \brief The comparison condition (e.g., ExactValue, GreaterThan).
  ScanComparison comparison = ScanComparison::kUnknown;

  /// \brief The primary value to search for.
  /// \details Used for ExactValue, GreaterThan, LessThan, IncreasedBy, etc.
  std::vector<std::byte> value;

  /// \brief The secondary value for range searches.
  /// \details Used only when comparison is kBetween or kNotBetween.
  std::vector<std::byte> value_end;

  /// \brief A bitmask for pattern matching.
  /// \details Must be the same size as `value`. Bytes set to 0x00 are ignored
  /// during comparison (wildcards).
  std::vector<std::byte> mask;

  /// \brief Memory alignment requirement.
  /// \details 1 = Byte aligned (slow, thorough), 4 = 4-byte aligned (fast).
  /// Typically matches the size of `value_type` for optimal performance.
  size_t alignment = 4;

  /// \brief Whether to use results from the previous scan.
  /// \details If true, this is a "Next Scan" that filters existing results.
  /// If false, this is a "First Scan" that searches all memory regions.
  bool use_previous_results = false;

  /// \brief Whether to suspend the target process during scanning.
  bool pause_while_scanning = false;

  /// \brief Validates the configuration for consistency.
  /// \return true if the configuration is valid, false otherwise.
  [[nodiscard]] bool Validate() const {
    if (alignment == 0) {
      return false;
    }

    size_t type_size = GetSizeForType(value_type);

    // For fixed-size types, the value buffer must match the type size.
    if (type_size > 0 && !value.empty() && value.size() != type_size) {
      return false;
    }

    // Mask must match value size if provided.
    if (!mask.empty() && mask.size() != value.size()) {
      return false;
    }

    // Range comparisons require a second value.
    if ((comparison == ScanComparison::kBetween ||
         comparison == ScanComparison::kNotBetween) &&
        value_end.empty()) {
      return false;
    }

    return true;
  }
};

}  // namespace maia::core
