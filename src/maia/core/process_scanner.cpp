// Copyright (c) Maia

#include "maia/core/process_scanner.h"

#include <algorithm>
#include <vector>

#include "maia/core/pattern_parser.h"
#include "maia/mmem/mmem.h"

namespace maia::core {

std::optional<uintptr_t> ScanData(std::span<const std::byte> data,
                                  uintptr_t address,
                                  size_t scan_size) {
  return ScanData(mmem::GetCurrentProcess(), data, address, scan_size);
}

std::optional<uintptr_t> ScanData(const mmem::ProcessDescriptor& process,
                                  std::span<const std::byte> data,
                                  uintptr_t address,
                                  size_t scan_size) {
  if (data.empty() || scan_size == 0) {
    return std::nullopt;
  }

  std::vector<std::byte> buffer(scan_size);
  size_t bytes_read = mmem::ReadMemory(process, address, buffer);
  if (bytes_read < data.size()) {
    return std::nullopt;
  }

  using DiffType = std::iter_difference_t<decltype(buffer.begin())>;

  // Adjust search range to avoid reading past the buffer.
  auto end_it = std::next(buffer.begin(), static_cast<DiffType>(bytes_read));
  if (bytes_read >= data.size()) {
    end_it = std::next(buffer.begin(),
                       static_cast<DiffType>(bytes_read - data.size() + 1));
  }

  auto it = std::search(buffer.begin(), end_it, data.begin(), data.end());

  if (it != end_it) {
    size_t offset = std::distance(buffer.begin(), it);
    return address + offset;
  }

  return std::nullopt;
}

std::optional<uintptr_t> ScanPattern(std::span<const std::byte> pattern,
                                     std::string_view mask,
                                     uintptr_t address,
                                     size_t scan_size) {
  return ScanPattern(
      mmem::GetCurrentProcess(), pattern, mask, address, scan_size);
}

std::optional<uintptr_t> ScanPattern(const mmem::ProcessDescriptor& process,
                                     std::span<const std::byte> pattern,
                                     std::string_view mask,
                                     uintptr_t address,
                                     size_t scan_size) {
  if (pattern.size() != mask.size() || scan_size == 0) {
    return std::nullopt;
  }

  std::vector<std::byte> buffer(scan_size);
  size_t bytes_read = mmem::ReadMemory(process, address, buffer);
  if (bytes_read < pattern.size()) {
    return std::nullopt;
  }

  for (size_t i = 0; i <= bytes_read - pattern.size(); i++) {
    bool match = true;
    for (size_t j = 0; j < pattern.size(); j++) {
      if (mask[j] == 'x' && buffer[i + j] != pattern[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return address + i;
    }
  }

  return std::nullopt;
}

std::optional<uintptr_t> ScanSignature(std::string_view signature,
                                       uintptr_t address,
                                       size_t scan_size) {
  return ScanSignature(
      mmem::GetCurrentProcess(), signature, address, scan_size);
}

std::optional<uintptr_t> ScanSignature(const mmem::ProcessDescriptor& process,
                                       std::string_view signature,
                                       uintptr_t address,
                                       size_t scan_size) {
  Pattern pattern = ParseAob(signature);

  if (pattern.value.empty()) {
    return std::nullopt;
  }

  // Convert generic Pattern mask (0xFF/0x00) to simple 'x'/'?' string mask
  // required by ScanPattern implementation.
  // TODO(marco): Update ScanPattern to use maia::Pattern directly for bit-level
  // masking.
  std::string string_mask;
  string_mask.reserve(pattern.mask.size());

  for (std::byte m : pattern.mask) {
    // If mask is 0xFF, it's 'x' (match). If 0x00, it's '?' (wildcard).
    // Note: ParseAob returns 0xFF for matches and 0x00 for wildcards.
    string_mask.push_back(m == std::byte{0xFF} ? 'x' : '?');
  }

  return ScanPattern(process, pattern.value, string_mask, address, scan_size);
}

}  // namespace maia::core
