// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "maia/core/scan_types.h"

namespace maia {

template <typename T>
struct FixedScanResult {
  using value_type = T;
  static constexpr bool kIsVariable = false;
  static constexpr size_t kSizeBytes = sizeof(T);

  std::vector<uintptr_t> addresses;

  explicit operator bool() const noexcept {
    return !addresses.empty();
  }
};

// Base for variable-length results (e.g., kString, kByteArray)
template <typename T>
struct VariableScanResult {
  using value_type = T;
  static constexpr bool kIsVariable = true;
  static constexpr size_t kSizeBytes = sizeof(T);

  struct Entry {
    uintptr_t address = 0;
    size_t length = 0;  // Length of this specific match
  };

  std::vector<Entry> entries;

  explicit operator bool() const noexcept {
    return !entries.empty();
  }
};

// Helper to pick the correct specialization.
// clang-format off
// template <ScanValueType VT> struct ScanResultTraits { using type = FixedScanResult<VT>; };

// template <> struct ScanResultTraits<ScanValueType::kString>    { using type = VariableScanResult<ScanValueType::kString>; };
// template <> struct ScanResultTraits<ScanValueType::kStringW>   { using type = VariableScanResult<ScanValueType::kStringW>; };
// template <> struct ScanResultTraits<ScanValueType::kByteArray> { using type = VariableScanResult<ScanValueType::kByteArray>; };

// clang-format on

// Type-safe result.
// template <typename VT>
// using ScanResultTyped = typename ScanResultTraits<VT>::type;

// // Variant of all possible results
// using ScanResult = std::variant<ScanResultTyped<ScanValueType::kS8>,
//                                 ScanResultTyped<ScanValueType::kU8>,
//                                 ScanResultTyped<ScanValueType::kS16>,
//                                 ScanResultTyped<ScanValueType::kU16>,
//                                 ScanResultTyped<ScanValueType::kS32>,
//                                 ScanResultTyped<ScanValueType::kU32>,
//                                 ScanResultTyped<ScanValueType::kF32>,
//                                 ScanResultTyped<ScanValueType::kS64>,
//                                 ScanResultTyped<ScanValueType::kU64>,
//                                 ScanResultTyped<ScanValueType::kF64>,
//                                 ScanResultTyped<ScanValueType::kString>,
//                                 ScanResultTyped<ScanValueType::kStringW>,
//                                 ScanResultTyped<ScanValueType::kByteArray>>;

// template <typename T>
// struct ScanResultType {};

using ScanResult = std::variant<FixedScanResult<int8_t>,
                                FixedScanResult<uint8_t>,
                                FixedScanResult<int16_t>,
                                FixedScanResult<uint16_t>,
                                FixedScanResult<int32_t>,
                                FixedScanResult<uint32_t>,
                                FixedScanResult<int64_t>,
                                FixedScanResult<uint64_t>,
                                FixedScanResult<float>,
                                FixedScanResult<double>,
                                VariableScanResult<std::string>,
                                VariableScanResult<std::wstring>,
                                VariableScanResult<std::vector<std::byte>>>;

// template <ScanValueType VT>
// ScanResult MakeScanResult(ScanResultTyped<VT>) {}

// // Helper: iterate addresses regardless of type (for UI)
// template <typename Func>
// void ForEachAddress(const ScanResult& result, Func callback) {
//   std::visit(
//       [&](const auto& typed) {
//         if constexpr (std::decay_t<decltype(typed)>::kIsVariable) {
//           for (const auto& entry : typed.entries) {
//             callback(entry.address);
//           }
//         } else {
//           for (uintptr_t addr : typed.addresses) {
//             callback(addr);
//           }
//         }
//       },
//       result);
// }

}  // namespace maia
