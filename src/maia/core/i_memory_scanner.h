// Copyright (c) Maia

#pragma once

#include <string>
#include <variant>
#include <vector>

#include "maia/core/scan_result.h"
#include "maia/core/scan_types.h"

namespace maia {

namespace detail {

struct VariableParamsBase {
  ScanComparison type = ScanComparison::kExactValue;
};

}  // namespace detail

template <typename T>
struct ScanParamsType {
  using value_type = T;

  ScanComparison comparison = ScanComparison::kUnknown;
  value_type value{};
  value_type upper_bound{};
};

template <>
struct ScanParamsType<std::string> : detail::VariableParamsBase {
  std::string pattern;
  bool case_sensitive = false;
};

template <>
struct ScanParamsType<std::wstring> : detail::VariableParamsBase {
  std::wstring pattern;
  bool case_sensitive = false;
};

template <>
struct ScanParamsType<std::vector<std::byte>> : detail::VariableParamsBase {
  std::vector<std::byte> pattern;
};

using ScanParams = std::variant<ScanParamsType<int8_t>,
                                ScanParamsType<uint8_t>,
                                ScanParamsType<int16_t>,
                                ScanParamsType<uint16_t>,
                                ScanParamsType<int32_t>,
                                ScanParamsType<uint32_t>,
                                ScanParamsType<int64_t>,
                                ScanParamsType<uint64_t>,
                                ScanParamsType<float>,
                                ScanParamsType<double>,
                                ScanParamsType<std::string>,
                                ScanParamsType<std::wstring>,
                                ScanParamsType<std::vector<std::byte>>>;

template <CScannableType T>
auto MakeScanParams(ScanComparison comparison, T value, T upper_bound = {}) {
  ScanParamsType<T> params;
  params.comparison = comparison;
  params.value = value;
  params.upper_bound = upper_bound;
  return params;
}

/// \brief Abstract interface for scanning process memory.
///
/// \note Implementations must retain a memory snapshot to back all returned
///       ScanResult objects. The snapshot must outlive any result referencing
///       it.
class IMemoryScanner {
 public:
  virtual ~IMemoryScanner() = default;

  /// \brief Performs an initial scan of the entire process memory.
  /// \param params Scan parameters (type, comparison, value).
  /// \return Addresses where matching values were found.
  virtual ScanResult NewScan(const ScanParams& params) = 0;

  /// \brief Filters a previous result by re-scanning its addresses.
  /// \param previous_result Result from a prior scan.
  /// \param params New scan parameters (e.g., kChanged, kExactValue).
  /// \return Filtered subset of addresses.
  virtual ScanResult NextScan(const ScanResult& previous_result,
                              const ScanParams& params) = 0;
};

}  // namespace maia
