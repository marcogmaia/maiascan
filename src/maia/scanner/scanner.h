// Copyright (c) Maia

#pragma once

#include <vector>

#include "maia/scanner/i_process_memory_accessor.h"

namespace maia::scanner {

/// \brief Performs memory scanning operations on a readable process.
///
/// \details This class is stateless. It queries an IProcessReader to find all
/// addresses matching a given pattern.
class Scanner {
 public:
  /// \brief Constructs a Scanner that will use the given reader.
  explicit Scanner(IProcessMemoryAccessor& process_accessor);

  /// \brief Performs an initial scan over all memory regions.
  ///
  /// \param value_to_find A span of bytes representing the value to search for.
  /// \return A vector of addresses where the value was found.
  std::vector<uintptr_t> ScanFor(std::span<const std::byte> value_to_find);

  /// \brief Performs a "next scan" by filtering an existing list of addresses.
  ///
  /// \details This is the core "narrowing" operation. It re-reads only the
  /// candidate addresses to see if they now match the new value.
  ///
  /// \param candidates A list of addresses from a previous scan. \param
  /// new_value The new value to check for at those addresses. \return A
  /// filtered vector of addresses that match the new value.
  std::vector<uintptr_t> ScanAddresses(const std::vector<uintptr_t>& candidates,
                                       std::span<const std::byte> new_value);

 private:
  IProcessMemoryAccessor& accessor_;

  // Internal buffer to avoid re-allocating on every read
  std::vector<std::byte> read_buffer_;
};

}  // namespace maia::scanner
