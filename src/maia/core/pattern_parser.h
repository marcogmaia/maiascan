// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace maia {

/// \brief A bitmask-based byte sequence for memory scanning.
///
/// Pattern represents a sequence where some bytes must match exactly and others
/// (wildcards) are ignored. This is implemented using two parallel vectors:
/// `value` contains the bytes to find, and `mask` defines the significance of
/// each bit.
struct Pattern {
  /// \brief The target byte values.
  /// For wildcard positions, the value is typically 0x00 but is ignored
  /// during comparison if the corresponding mask byte is 0x00.
  std::vector<std::byte> value;

  /// \brief The matching bitmask.
  /// Each byte corresponds to the same index in `value`.
  /// - 0xFF: The byte must match exactly.
  /// - 0x00: The byte is a wildcard (always matches).
  std::vector<std::byte> mask;
};

/// \brief Parses an Array of Bytes string (e.g., "AA BB ?? DD").
/// \param aob_str The hex string with optional wildcards (?? or ?).
Pattern ParseAob(std::string_view aob_str);

/// \brief Converts a text string to a byte pattern.
/// \param text The text to search for.
/// \param is_utf16 If true, converts to UTF-16LE. Otherwise UTF-8.
Pattern ParseText(std::string_view text, bool is_utf16);

}  // namespace maia
