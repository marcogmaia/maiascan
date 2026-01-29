// Copyright (c) Maia

#include "maia/core/pattern_parser.h"

#include <cctype>
#include <optional>

namespace maia {

namespace {

std::optional<uint8_t> HexToByte(char c) {
  // clang-format off
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  // clang-format on
  return std::nullopt;  // Clearly indicates failure
}

}  // namespace

Pattern ParseAob(std::string_view aob_str) {
  // TODO: Expand ParseAob to support bit-level or nibble-level masking.
  // This would allow patterns like "B?" for nibble wildcards (Value 0xB0, Mask
  // 0xF0) or "C0/F8" for explicit bitmasks. The Pattern struct and scanners
  // already support this via the parallel mask vector.
  Pattern pattern;
  pattern.value.reserve(aob_str.size() / 2);
  pattern.mask.reserve(aob_str.size() / 2);

  while (!aob_str.empty()) {
    const char c = aob_str.front();

    // Skip Delimiters
    // We can safely cast because we know input is not empty.
    if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
      aob_str.remove_prefix(1);
      continue;
    }

    // Handle Wildcards (? or ??).
    if (c == '?') {
      pattern.value.emplace_back(std::byte{0});
      pattern.mask.emplace_back(
          std::byte{0});  // 0x00 mask means "ignore this byte"

      aob_str.remove_prefix(1);
      // If the next char is also '?', consume it too (treat "?? " same as "? ")
      if (!aob_str.empty() && aob_str.front() == '?') {
        aob_str.remove_prefix(1);
      }
      continue;
    }

    // Handle String Literals ("text").
    if (c == '"') {
      // Find closing quote starting from index 1 (skip the opening quote)
      size_t end_pos = aob_str.find('"', 1);

      if (end_pos != std::string_view::npos) {
        // Extract the content inside quotes
        std::string_view text = aob_str.substr(1, end_pos - 1);

        for (char tc : text) {
          pattern.value.emplace_back(static_cast<std::byte>(tc));
          pattern.mask.emplace_back(std::byte{0xFF});
        }

        // Remove everything up to and including the closing quote
        aob_str.remove_prefix(end_pos + 1);
      } else {
        // Malformed: Quote opened but never closed.
        // Skip the opening quote to avoid infinite loop.
        aob_str.remove_prefix(1);
      }
      continue;
    }

    // Handle Hex Bytes (AB or A).
    if (auto high = HexToByte(c); high) {
      uint8_t byte_val = *high;
      aob_str.remove_prefix(1);  // Consume high nibble

      // Check if there is a low nibble available
      if (!aob_str.empty()) {
        if (auto low = HexToByte(aob_str.front()); low) {
          byte_val = (byte_val << 4) | *low;
          aob_str.remove_prefix(1);  // Consume low nibble
        }
      }

      pattern.value.emplace_back(static_cast<std::byte>(byte_val));
      pattern.mask.emplace_back(std::byte{0xFF});
      continue;
    }

    // Unknown garbage: Skip it safely.
    aob_str.remove_prefix(1);
  }

  return pattern;
}

Pattern ParseText(std::string_view text, bool is_utf16) {
  Pattern pattern;
  if (is_utf16) {
    // Very basic UTF-16LE conversion (works for ASCII range)
    for (char c : text) {
      pattern.value.emplace_back(static_cast<std::byte>(c));
      pattern.value.emplace_back(std::byte{0});
      pattern.mask.emplace_back(std::byte{0xFF});
      pattern.mask.emplace_back(std::byte{0xFF});
    }
  } else {
    for (char c : text) {
      pattern.value.emplace_back(static_cast<std::byte>(c));
      pattern.mask.emplace_back(std::byte{0xFF});
    }
  }
  return pattern;
}

}  // namespace maia
