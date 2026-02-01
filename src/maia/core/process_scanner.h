// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "maia/mmem/mmem.h"

namespace maia::core {

// Scan API

// Scans for raw data in the current process.
std::optional<uintptr_t> ScanData(std::span<const std::byte> data,
                                  uintptr_t address,
                                  size_t scan_size);

// Scans for raw data in a process.
std::optional<uintptr_t> ScanData(const mmem::ProcessDescriptor& process,
                                  std::span<const std::byte> data,
                                  uintptr_t address,
                                  size_t scan_size);

// Scans for pattern/mask in the current process.
std::optional<uintptr_t> ScanPattern(std::span<const std::byte> pattern,
                                     std::string_view mask,
                                     uintptr_t address,
                                     size_t scan_size);

// Scans for pattern/mask in a process.
std::optional<uintptr_t> ScanPattern(const mmem::ProcessDescriptor& process,
                                     std::span<const std::byte> pattern,
                                     std::string_view mask,
                                     uintptr_t address,
                                     size_t scan_size);

// Scans for hexadecimal signature string (e.g., "DE AD BE EF ?? ?? 13 37") in
// the current process.
std::optional<uintptr_t> ScanSignature(std::string_view signature,
                                       uintptr_t address,
                                       size_t scan_size);

// Scans for hexadecimal signature string in a process.
std::optional<uintptr_t> ScanSignature(const mmem::ProcessDescriptor& process,
                                       std::string_view signature,
                                       uintptr_t address,
                                       size_t scan_size);

}  // namespace maia::core
