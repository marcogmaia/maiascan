// Copyright (c) Maia

#pragma once

#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maia::core {

// Helper to trim whitespace from the beginning and end of a string.
inline std::string_view Trim(std::string_view str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) {
    return "";
  }
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

// Helper to split string by delimiter.
inline std::vector<std::string_view> Split(std::string_view str,
                                           char delimiter) {
  std::vector<std::string_view> result;
  size_t start = 0;
  size_t end = str.find(delimiter);
  while (end != std::string_view::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }
  result.push_back(str.substr(start));
  return result;
}

// Generic number parser using std::from_chars.
// Supports integer and floating point types.
// Returns std::nullopt on failure or if the entire string wasn't consumed.
template <typename T>
std::optional<T> ParseNumber(std::string_view str, int base = 10) {
  str = Trim(str);
  if (str.empty()) {
    return std::nullopt;
  }

  // Handle 0x prefix for hex
  if (base == 16 && str.size() > 2 && str.starts_with("0x")) {
    str = str.substr(2);
  } else if (base == 0) {
    // Auto-detect base
    if (str.size() > 2 && str.starts_with("0x")) {
      base = 16;
      str = str.substr(2);
    } else {
      base = 10;
    }
  }

  const char* first = str.data();
  const char* last = first + str.size();

  T value;
  std::from_chars_result result;

  if constexpr (std::is_floating_point_v<T>) {
    // std::from_chars for float doesn't take base
    result = std::from_chars(first, last, value);
  } else {
    result = std::from_chars(first, last, value, base);
  }

  if (result.ec != std::errc() || result.ptr != last) {
    return std::nullopt;
  }
  return value;
}

// Generic number-to-string converter using std::to_chars.
// Converts integers to string in specified base (default 10).
// Returns empty string on failure.
template <typename T>
std::string ToString(T value, int base = 10) {
  std::string result;
  result.resize(sizeof(T) * 8 + 1);  // Allocate enough space
  std::to_chars_result chars_result =
      std::to_chars(result.data(), result.data() + result.size(), value, base);
  if (chars_result.ec != std::errc()) {
    return "";
  }
  result.resize(chars_result.ptr - result.data());
  return result;
}

// Convenience function to convert to hexadecimal string.
// Returns lowercase hex by default. Set uppercase to true for uppercase.
template <typename T>
std::string ToHexString(T value, bool uppercase = false) {
  std::string result = ToString(value, 16);
  if (uppercase) {
    for (char& c : result) {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
  }
  return result;
}

}  // namespace maia::core
