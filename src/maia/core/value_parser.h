// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "maia/core/pattern_parser.h"
#include "maia/core/scan_types.h"
#include "maia/core/string_utils.h"

namespace maia {

template <typename T>
std::optional<T> ParseValue(std::string_view sview, int base = 10) {
  return core::ParseNumber<T>(sview, base);
}

template <typename T>
std::vector<std::byte> ToByteVector(T value) {
  std::vector<std::byte> bytes(sizeof(T));
  std::memcpy(bytes.data(), &value, sizeof(T));
  return bytes;
}

template <typename T>
std::vector<std::byte> NumberStrToBytes(std::string_view str, int base) {
  return ParseValue<T>(str, base)
      .transform(ToByteVector<T>)
      .value_or(std::vector<std::byte>{});
}

inline std::vector<std::byte> ParseStringByType(std::string_view str,
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
    case ScanValueType::kString: {
      std::vector<std::byte> bytes(str.size());
      std::memcpy(bytes.data(), str.data(), str.size());
      return bytes;
    }
    default:
      return {};
  }
}

inline Pattern ParsePatternByType(std::string_view str,
                                  ScanValueType type,
                                  int base = 10) {
  if (type == ScanValueType::kArrayOfBytes) {
    return ParseAob(str);
  }
  if (type == ScanValueType::kString) {
    return ParseText(str, false);
  }
  if (type == ScanValueType::kWString) {
    return ParseText(str, true);
  }

  // For numeric types, use the standard parser and create a full mask
  Pattern p;
  p.value = ParseStringByType(str, type, base);
  p.mask.resize(p.value.size(), std::byte{0xFF});
  return p;
}

}  // namespace maia
