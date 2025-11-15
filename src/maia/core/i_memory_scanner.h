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

}  // namespace maia
