// Copyright (c) Maia

#pragma once

#include <span>
#include <vector>

#include "maia/scanner/memory_common.h"

namespace maia::scanner {

struct MemoryRegion {
  // TODO: Add a constructor to simplify the creation.

  size_t base_address;

  // View over the entire memory region.
  std::span<std::byte> view;

  uint32_t protection_flags = 0;  // e.g., PAGE_READWRITE
  uint32_t state = 0;             // e.g., MEM_COMMIT

  void* data() const {
    return view.data();
  }

  size_t size() const {
    return view.size();
  }
};

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
