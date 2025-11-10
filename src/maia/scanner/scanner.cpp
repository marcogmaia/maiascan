// Copyright (c) Maia

#include <Windows.h>

#include <Psapi.h>
#include <TlHelp32.h>

#include <algorithm>

#include "maia/scanner/scanner.h"

#include <fmt/core.h>

#include "maia/core/memory_common.h"
#include "maia/core/memory_protection.h"

namespace maia::scanner {

std::vector<uintptr_t> Scanner::ScanFor(
    std::span<const std::byte> value_to_find) {
  std::vector<uintptr_t> results;

  if (value_to_find.empty()) {
    return results;
  }

  // Get all memory regions from the process
  auto regions = memory_accessor_.GetMemoryRegions();

  for (const auto& region : regions) {
    // Skip non-readable regions.
    if (!IsReadable(region.protection_flags)) {
      continue;
    }

    // Read the entire region
    read_buffer_.resize(region.size);
    if (!memory_accessor_.ReadMemory(std::bit_cast<void*>(region.base_address),
                                     read_buffer_)) {
      continue;
    }

    // Search for the value pattern in this region
    auto it = read_buffer_.begin();
    while (true) {
      it = std::search(
          it, read_buffer_.end(), value_to_find.begin(), value_to_find.end());

      if (it >= read_buffer_.end()) {
        break;
      }

      size_t offset = std::distance(read_buffer_.begin(), it);
      results.push_back(region.base_address + offset);

      // Move past this match to find next one
      std::advance(it, value_to_find.size());
    }
  }

  return results;
}

std::vector<uintptr_t> Scanner::ScanAddresses(
    const std::vector<uintptr_t>& candidates,
    std::span<const std::byte> new_value) {
  std::vector<uintptr_t> results;

  if (new_value.empty() || candidates.empty()) {
    return results;
  }

  read_buffer_.resize(new_value.size());

  for (uintptr_t address : candidates) {
    // Read current value at this address
    if (memory_accessor_.ReadMemory(reinterpret_cast<void*>(address),
                                    read_buffer_)) {
      // Compare with what we're looking for
      if (std::equal(read_buffer_.begin(),
                     read_buffer_.end(),
                     new_value.begin(),
                     new_value.end())) {
        results.push_back(address);
      }
    }
  }

  return results;
}

}  // namespace maia::scanner
