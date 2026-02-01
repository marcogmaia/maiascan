// Copyright (c) Maia

#pragma once

#include <span>
#include <string>

#include "maia/core/scan_types.h"

namespace maia {

class ValueFormatter {
 public:
  /// \brief Formats a raw byte span into a string representation based on type.
  /// \param data The raw bytes to format.
  /// \param type The type to interpret the bytes as.
  /// \param is_hex If true, formats numbers in hexadecimal.
  /// \return String representation of the value.
  static std::string Format(std::span<const std::byte> data,
                            ScanValueType type,
                            bool is_hex = false);

  /// \brief Returns a user-friendly label for a ScanValueType.
  static const char* GetLabel(ScanValueType type);

  /// \brief Returns a user-friendly label for a ScanComparison.
  static const char* GetLabel(ScanComparison comparison);
};

}  // namespace maia
