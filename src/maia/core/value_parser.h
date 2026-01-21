// Copyright (c) Maia

#pragma once

#include <charconv>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "maia/core/scan_types.h"

namespace maia {

template <typename T>
std::optional<T> ParseValue(std::string_view sview, int base = 10) {
  if (base == 16 && sview.starts_with("0x")) {
    sview = sview.substr(2);
  }
  const char* first = sview.data();
  const char* last = first + sview.size();

  T value;
  std::from_chars_result result;

  if constexpr (std::is_floating_point_v<T>) {
    result = std::from_chars(first, last, value);
  } else {
    result = std::from_chars(first, last, value, base);
  }

  if (result.ec != std::errc() || result.ptr != last) {
    return std::nullopt;
  }
  return value;
}

template <typename T>
std::vector<std::byte> ToByteVector(T value) {
  std::vector<std::byte> bytes(sizeof(T));
  std::memcpy(bytes.data(), &value, sizeof(T));
  return bytes;
}

template <typename T>
std::vector<std::byte> NumberStrToBytes(const std::string& str, int base) {
  return ParseValue<T>(str, base)
      .transform(ToByteVector<T>)
      .value_or(std::vector<std::byte>{});
}

inline std::vector<std::byte> ParseStringByType(const std::string& str,
                                                ScanValueType type,
                                                int base = 10) {
  switch (type) {
    case ScanValueType::kInt8:
      return NumberStrToBytes<int8_t>(str, base);
    case ScanValueType::kUInt8:
      return NumberStrToBytes<uint8_t>(str, base);
    case ScanValueType::kInt16:
      return NumberStrToBytes<int16_t>(str, base);
    case ScanValueType::kUInt16:
      return NumberStrToBytes<uint16_t>(str, base);
    case ScanValueType::kInt32:
      return NumberStrToBytes<int32_t>(str, base);
    case ScanValueType::kUInt32:
      return NumberStrToBytes<uint32_t>(str, base);
    case ScanValueType::kInt64:
      return NumberStrToBytes<int64_t>(str, base);
    case ScanValueType::kUInt64:
      return NumberStrToBytes<uint64_t>(str, base);
    case ScanValueType::kFloat:
      return NumberStrToBytes<float>(str, base);
    case ScanValueType::kDouble:
      return NumberStrToBytes<double>(str, base);
    default:
      return {};
  }
}

}  // namespace maia
