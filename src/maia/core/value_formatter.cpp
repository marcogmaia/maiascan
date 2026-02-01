// Copyright (c) Maia

#include "maia/core/value_formatter.h"

#include <cstring>
#include <format>

namespace maia {

namespace {

struct Utf8String {};

struct Utf16String {};

struct ByteArray {};

template <typename T>
std::string FormatTypedValue(std::span<const std::byte> data, bool is_hex) {
  if (data.size() < sizeof(T)) {
    return "Invalid";
  }
  T val;
  std::memcpy(&val, data.data(), sizeof(T));

  if constexpr (std::is_floating_point_v<T>) {
    return std::format("{:.6f}", val);
  } else {
    if (is_hex) {
      constexpr size_t kHexWidth = 2 * sizeof(T);
      return std::format("0x{:0{}x}", val, kHexWidth);
    } else {
      return std::format("{}", val);
    }
  }
}

template <>
std::string FormatTypedValue<Utf8String>(std::span<const std::byte> data,
                                         bool /*is_hex*/) {
  if (data.empty()) {
    return "";
  }
  // Treat as null-terminated if it contains nulls, or just raw chars
  // For safety, let's just construct from span size
  const char* ptr = reinterpret_cast<const char*>(data.data());
  size_t len = 0;
  while (len < data.size() && ptr[len] != '\0') {
    ++len;
  }
  return std::string(ptr, len);
}

template <>
std::string FormatTypedValue<Utf16String>(std::span<const std::byte> data,
                                          bool /*is_hex*/) {
  // Basic UTF-16LE to UTF-8 conversion for display
  std::string text;
  text.reserve(data.size() / 2);
  for (size_t i = 0; i + 1 < data.size(); i += 2) {
    char c = static_cast<char>(data[i]);
    text += (c == '\0') ? ' ' : c;  // Replace nulls/non-ascii roughly
  }
  return text;
}

template <>
std::string FormatTypedValue<ByteArray>(std::span<const std::byte> data,
                                        bool /*is_hex*/) {
  std::string text;
  text.reserve(data.size() * 3);
  for (size_t i = 0; i < data.size(); ++i) {
    text += std::format("{:02X}{}",
                        static_cast<uint8_t>(data[i]),
                        (i == data.size() - 1) ? "" : " ");
  }
  return text;
}

}  // namespace

std::string ValueFormatter::Format(std::span<const std::byte> data,
                                   ScanValueType type,
                                   bool is_hex) {
  if (data.empty()) {
    return "N/A";
  }

  switch (type) {
    case ScanValueType::kInt8:
      return FormatTypedValue<int8_t>(data, is_hex);
    case ScanValueType::kUInt8:
      return FormatTypedValue<uint8_t>(data, is_hex);
    case ScanValueType::kInt16:
      return FormatTypedValue<int16_t>(data, is_hex);
    case ScanValueType::kUInt16:
      return FormatTypedValue<uint16_t>(data, is_hex);
    case ScanValueType::kInt32:
      return FormatTypedValue<int32_t>(data, is_hex);
    case ScanValueType::kUInt32:
      return FormatTypedValue<uint32_t>(data, is_hex);
    case ScanValueType::kInt64:
      return FormatTypedValue<int64_t>(data, is_hex);
    case ScanValueType::kUInt64:
      return FormatTypedValue<uint64_t>(data, is_hex);
    case ScanValueType::kFloat:
      return FormatTypedValue<float>(data, false);
    case ScanValueType::kDouble:
      return FormatTypedValue<double>(data, false);
    case ScanValueType::kString:
      return FormatTypedValue<Utf8String>(data, false);
    case ScanValueType::kWString:
      return FormatTypedValue<Utf16String>(data, false);
    case ScanValueType::kArrayOfBytes:
      return FormatTypedValue<ByteArray>(data, false);
  }
  return "Unknown";
}

const char* ValueFormatter::GetLabel(ScanValueType type) {
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
    case ScanValueType::kString:
      return "String";
    case ScanValueType::kWString:
      return "Unicode String";
    case ScanValueType::kArrayOfBytes:
      return "Array of Bytes";
  }
  return "Unknown";
}

const char* ValueFormatter::GetLabel(ScanComparison comparison) {
  switch (comparison) {
    case ScanComparison::kUnknown:
      return "Unknown";
    case ScanComparison::kExactValue:
      return "Exact Value";
    case ScanComparison::kNotEqual:
      return "Not Equal";
    case ScanComparison::kGreaterThan:
      return "Greater Than";
    case ScanComparison::kLessThan:
      return "Less Than";
    case ScanComparison::kBetween:
      return "Between";
    case ScanComparison::kNotBetween:
      return "Not Between";
    case ScanComparison::kChanged:
      return "Changed";
    case ScanComparison::kUnchanged:
      return "Unchanged";
    case ScanComparison::kIncreased:
      return "Increased";
    case ScanComparison::kDecreased:
      return "Decreased";
    case ScanComparison::kIncreasedBy:
      return "Increased By";
    case ScanComparison::kDecreasedBy:
      return "Decreased By";
  }
  return "Unknown";
}

}  // namespace maia
