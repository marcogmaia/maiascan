// Copyright (c) Maia

#pragma once

#include <concepts>
#include <vector>

namespace maia {

enum class ScanComparison {
  // kInvalid,

  // === Initial Scan Types ===
  // These compare memory against a specific value (or unknown).

  // Used for the very first scan when the value is not known.
  // This scan typically snapshots all memory regions.
  kUnknown,

  // Scans for a precise value.
  // (Memory == Value)
  kExactValue,

  // Scans for any value *except* the specified one.
  // (Memory != Value)
  kNotEqual,

  // Scans for values greater than the specified one.
  // (Memory > Value)
  kGreaterThan,

  // Scans for values less than the specified one.
  // (Memory < Value)
  kLessThan,

  // Scans for values within a specified range (inclusive).
  // (Value1 <= Memory <= Value2)
  kBetween,

  // Scans for values outside a specified range.
  // (Memory < Value1 OR Memory > Value2)
  kNotBetween,

  // === Subsequent Scan Types ===
  // These compare the current memory value against the value
  // from the *previous* scan snapshot.

  // Keeps addresses where the value has changed.
  // (CurrentMemory != PreviousMemory)
  kChanged,

  // Keeps addresses where the value has not changed.
  // (CurrentMemory == PreviousMemory)
  kUnchanged,

  // Keeps addresses where the value has increased.
  // (CurrentMemory > PreviousMemory)
  kIncreased,

  // Keeps addresses where the value has decreased.
  // (CurrentMemory < PreviousMemory)
  kDecreased,

  // Keeps addresses where the value increased by a specific amount.
  // (CurrentMemory == PreviousMemory + Value)
  kIncreasedBy,

  // Keeps addresses where the value decreased by a specific amount.
  // (CurrentMemory == PreviousMemory - Value)
  kDecreasedBy,
};

// Helper to check which inputs the UI should show (0, 1, or 2 values)
constexpr int GetRequiredValueCount(ScanComparison type) {
  switch (type) {
    // These need two values (e.g., "Value 1" and "Value 2")
    case ScanComparison::kBetween:
    case ScanComparison::kNotBetween:
      return 2;

    // These need one value
    case ScanComparison::kExactValue:
    case ScanComparison::kNotEqual:
    case ScanComparison::kGreaterThan:
    case ScanComparison::kLessThan:
    case ScanComparison::kIncreasedBy:
    case ScanComparison::kDecreasedBy:
      return 1;

    // These need no user-provided value
    case ScanComparison::kUnknown:
    case ScanComparison::kChanged:
    case ScanComparison::kUnchanged:
    case ScanComparison::kIncreased:
    case ScanComparison::kDecreased:
    default:
      return 0;
  }
}

// Helper to check if this scan is valid as a "First Scan"
constexpr bool IsValidForFirstScan(ScanComparison type) {
  switch (type) {
    case ScanComparison::kUnknown:
    case ScanComparison::kExactValue:
    case ScanComparison::kNotEqual:
    case ScanComparison::kGreaterThan:
    case ScanComparison::kLessThan:
    case ScanComparison::kBetween:
    case ScanComparison::kNotBetween:
      return true;
    default:
      return false;
  }
}

template <typename T>
concept CScannableType = std::integral<T> || std::floating_point<T>;

enum class ScanValueType {
  kInt8,
  kUInt8,
  kInt16,
  kUInt16,
  kInt32,
  kUInt32,
  kInt64,
  kUInt64,
  kFloat,
  kDouble
};

struct ScanStorage {
  std::vector<uintptr_t> addresses;
  std::vector<std::byte> curr_raw;
  std::vector<std::byte> prev_raw;
  size_t stride;
};

}  // namespace maia
