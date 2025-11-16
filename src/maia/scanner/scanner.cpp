// Copyright (c) Maia

#include <Windows.h>

#include <Psapi.h>
#include <TlHelp32.h>

#include <search.h>  // For std::search
#include <algorithm>
#include <iterator>  // For std::distance, std::advance

#include "maia/scanner/scanner.h"

#include <fmt/core.h>

#include "maia/core/memory_common.h"

// #include "maia/core/memory_protection.h"

namespace maia::scanner {

namespace {

constexpr bool IsReadable(mmem::Protection prot) noexcept {
  return static_cast<bool>(static_cast<uint32_t>(prot) &
                           static_cast<uint32_t>(mmem::Protection::kRead));
}

}  // namespace

std::vector<uintptr_t> Scanner::ScanFor(
    std::span<const std::byte> value_to_find) {
  std::vector<uintptr_t> results;

  if (value_to_find.empty()) {
    return results;
  }

  auto regions = memory_accessor_.GetMemoryRegions();

  for (const auto& region : regions) {
    if (!IsReadable(region.protection)) {
      continue;
    }

    // Ensure our buffer is the correct size for the region
    read_buffer_.resize(region.size);

    // Corrected ReadMemory call:
    // We create a span of 1 address (region.base)
    // and ask to read region.size bytes from it.
    const MemoryAddress addr = region.base;
    if (!memory_accessor_.ReadMemory(
            std::span<const MemoryAddress>{&addr, 1},  // Address to read
            region.size,                               // Number of bytes
            read_buffer_)) {                           // Output buffer
      continue;
    }

    // Search for the value pattern in the buffer
    auto it = read_buffer_.begin();
    while (true) {
      it = std::search(
          it, read_buffer_.end(), value_to_find.begin(), value_to_find.end());

      if (it >= read_buffer_.end()) {
        break;
      }

      size_t offset = std::distance(read_buffer_.begin(), it);
      results.push_back(region.base + offset);

      // Move past this match to find the next one
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

  const size_t value_size = new_value.size();
  const size_t num_candidates = candidates.size();

  // Resize the buffer once to hold all results.
  read_buffer_.resize(num_candidates * value_size);

  // --- Optimized Batch Read ---
  // Call ReadMemory ONCE, passing all candidate addresses.
  // The IProcess implementation will handle batching these.
  // We don't need to check the return 'bool', as the buffer
  // will be populated on a best-effort basis (even with fallbacks),
  // and any failed reads will simply fail the comparison check.
  memory_accessor_.ReadMemory(candidates,   // Span-compatible list of addresses
                              value_size,   // Bytes to read at each address
                              read_buffer_  // The large output buffer
  );
  // --- End of Update ---

  // Check the results in our local buffer
  for (size_t i = 0; i < num_candidates; ++i) {
    // Get the slice of the buffer for this candidate
    auto current_value_span =
        std::span{read_buffer_}.subspan(i * value_size, value_size);

    // Compare with what we're looking for
    if (std::equal(current_value_span.begin(),
                   current_value_span.end(),
                   new_value.begin(),
                   new_value.end())) {
      results.push_back(candidates[i]);
    }
  }

  return results;
}

}  // namespace maia::scanner
