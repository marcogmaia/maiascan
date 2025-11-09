// Copyright (c) Maia

#pragma once

#include <string>
#include <vector>

#include "maia/core/scan_types.h"

#include "scan_result.h"

namespace maia {

namespace detail {

// --- A base struct for common variable-type parameters ---
struct VariableParamsBase {
  static constexpr bool kIsVariable = true;
  ScanComparison type = ScanComparison::kExactValue;
};

}  // namespace detail

template <typename T>
struct ScanParamsTyped {
  using value_type = ScanValueType<T>::value_type;

  static constexpr bool kIsVariable = false;
  ScanComparison comparison = ScanComparison::kUnknown;
  value_type value{};
  value_type upper_bound{};
};

template <>
struct ScanParamsTyped<std::string> : detail::VariableParamsBase {
  std::string pattern;
  bool case_sensitive = false;
};

template <>
struct ScanParamsTyped<std::wstring> : detail::VariableParamsBase {
  std::wstring pattern;
  bool case_sensitive = false;
};

template <>
struct ScanParamsTyped<std::vector<std::byte>> : detail::VariableParamsBase {
  std::vector<std::byte> pattern;
};

using ScanParams = std::variant<ScanParamsTyped<int8_t>,
                                ScanParamsTyped<uint8_t>,
                                ScanParamsTyped<int16_t>,
                                ScanParamsTyped<uint16_t>,
                                ScanParamsTyped<int32_t>,
                                ScanParamsTyped<uint32_t>,
                                ScanParamsTyped<int64_t>,
                                ScanParamsTyped<uint64_t>,
                                ScanParamsTyped<float>,
                                ScanParamsTyped<double>,
                                ScanParamsTyped<std::string>,
                                ScanParamsTyped<std::wstring>,
                                ScanParamsTyped<std::vector<std::byte>>>;

// ------------------------------------------------------------------
// Type mapping: C++ type → enum
// ------------------------------------------------------------------
// clang-format off
// template <typename T>
// struct TypeToScanValueType {};  // Primary template (empty)

// // 8-bit
// template <> struct TypeToScanValueType<int8_t>   { static constexpr auto value = ScanValueType::kS8; };
// template <> struct TypeToScanValueType<uint8_t>  { static constexpr auto value = ScanValueType::kU8; };

// // 16-bit
// template <> struct TypeToScanValueType<int16_t>  { static constexpr auto value = ScanValueType::kS16; };
// template <> struct TypeToScanValueType<uint16_t> { static constexpr auto value = ScanValueType::kU16; };

// // 32-bit
// template <> struct TypeToScanValueType<int32_t>  { static constexpr auto value = ScanValueType::kS32; };
// template <> struct TypeToScanValueType<uint32_t> { static constexpr auto value = ScanValueType::kU32; };

// // 64-bit
// template <> struct TypeToScanValueType<int64_t>  { static constexpr auto value = ScanValueType::kS64; };
// template <> struct TypeToScanValueType<uint64_t> { static constexpr auto value = ScanValueType::kU64; };

// // Floating point
// template <> struct TypeToScanValueType<float>    { static constexpr auto value = ScanValueType::kF32; };
// template <> struct TypeToScanValueType<double>   { static constexpr auto value = ScanValueType::kF64; };

// // Variable types (for completeness, though factory won't work directly)
// template <> struct TypeToScanValueType<std::string>          { static constexpr auto value = ScanValueType::kString; };
// template <> struct TypeToScanValueType<std::wstring>         { static constexpr auto value = ScanValueType::kStringW; };
// template <> struct TypeToScanValueType<std::vector<uint8_t>> { static constexpr auto value = ScanValueType::kByteArray; };

// clang-format on

template <typename T>
auto MakeScanParams(ScanComparison comparison, T value1, T value2 = {}) {
  // TODO: Make this static_assert into a requires/concept.
  static_assert(!std::is_same_v<T, std::string> &&
                    !std::is_same_v<T, std::wstring> &&
                    !std::is_same_v<T, std::vector<uint8_t>>,
                "Use MakeScanParams for fundamental types only. "
                "For strings/byte arrays, construct ScanParamsTyped directly.");
  ScanParamsTyped<T> params;
  params.comparison = comparison;
  params.value1 = value1;
  params.value2 = value2;
  return params;
}

template <typename T>
struct ScanParamValueGetter;

// clang-format off

// template <> struct ScanParamValueGetter<uint32_t> { static constexpr ScanValueType kType = ScanValueType::kU32; };

// clang-format on

template <typename T>
T GetValue(const ScanParams& params) {
  return std::get<ScanParamsTyped<ScanParamValueGetter<T>::kType>>(params)
      .get();
}

// To implement the IMemoryScanner properly, for us to be able to return the
// ScanResult, it needs to store the snapshot of the memory, so the ScanResult
// can reference to it.

/// \brief Defines a contract for performing memory scanning operations against
/// a specific process.
class IMemoryScanner {
 public:
  virtual ~IMemoryScanner() = default;

  /// \brief Performs a new scan on the entire process memory. This is used to
  /// start a new "search."
  ///
  /// \param value_type The type of data to scan for (e.g., kU32, kString).
  /// \param params The scan parameters (comparison type, values).
  /// \return A ScanResultSet containing all found addresses.
  virtual ScanResult NewScan(/* ScanValueType value_type, */
                             const ScanParams& params) = 0;

  /// \brief Filters an existing scan result based on new criteria. This is used
  /// for all "next scans." It re-reads the memory at each address in
  /// previous_set and applies the new comparison.
  ///
  /// \param previous_set The result set from the previous scan.
  /// \param params The new scan parameters (e.g., kChanged, kExactValue).
  /// \return A new, filtered ScanResultSet.
  virtual ScanResult NextScan(const ScanResult& previous_result,
                              const ScanParams& params) = 0;
};

}  // namespace maia
