// Copyright (c) Maia

#pragma once

#include <concepts>
#include <string>
#include <variant>
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

constexpr const char* GetScanValueTypeName(ScanValueType type) {
  switch (type) {
    case ScanValueType::kInt8:
      return "Int8";
    case ScanValueType::kUInt8:
      return "UInt8";
    case ScanValueType::kInt16:
      return "Int16";
    case ScanValueType::kUInt16:
      return "UInt16";
    case ScanValueType::kInt32:
      return "Int32";
    case ScanValueType::kUInt32:
      return "UInt32";
    case ScanValueType::kInt64:
      return "Int64";
    case ScanValueType::kUInt64:
      return "UInt64";
    case ScanValueType::kFloat:
      return "Float";
    case ScanValueType::kDouble:
      return "Double";
  }
  return "Unknown";
}

// Base for variable-length results (e.g., kString, kByteArray)/
// template <typename T>
// struct VariableScanResult {
//   using value_type = T;
//   static constexpr bool kIsVariable = true;
//   static constexpr size_t kSizeBytes = sizeof(T);

//   struct Entry {
//     uintptr_t address = 0;
//     size_t length = 0;  // Length of this specific match
//   };

//   std::vector<Entry> entries;

//   explicit operator bool() const noexcept {
//     return !entries.empty();
//   }
// };

// using ScanResult = std::variant<FixedScanResult<int8_t>,
//                                 FixedScanResult<uint8_t>,
//                                 FixedScanResult<int16_t>,
//                                 FixedScanResult<uint16_t>,
//                                 FixedScanResult<int32_t>,
//                                 FixedScanResult<uint32_t>,
//                                 FixedScanResult<int64_t>,
//                                 FixedScanResult<uint64_t>,
//                                 FixedScanResult<float>,
//                                 FixedScanResult<double>,
//                                 VariableScanResult<std::string>,
//                                 VariableScanResult<std::wstring>,
//                                 VariableScanResult<std::vector<std::byte>>>;

}  // namespace maia
