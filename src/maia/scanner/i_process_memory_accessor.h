// Copyright (c) Maia

#pragma once

#include <span>
#include <vector>

#include "maia/scanner/memory_common.h"

namespace maia::scanner {

class IProcessMemoryAccessor {
 public:
  virtual ~IProcessMemoryAccessor() = default;

  /// \brief Gets a list of all scannable memory regions.
  /// \return A vector of MemoryRegion objects.
  virtual std::vector<MemoryRegion> GetMemoryRegions() = 0;

  /// \brief Reads a block of memory from a given virtual address.
  ///
  /// \param address The virtual address to read from.
  /// \param out_buffer A span of memory to write the data into.
  /// \return True if the read was successful, false otherwise.
  virtual bool ReadMemory(MemoryPtr address,
                          std::span<std::byte> out_buffer) = 0;

  /// \brief Writes a block of memory to a given virtual address.
  /// \return True if the write was successful, false otherwise.
  virtual bool WriteMemory(MemoryPtr address,
                           std::span<const std::byte> data) = 0;
};

}  // namespace maia::scanner
