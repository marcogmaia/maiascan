// Copyright (c) Maia

#pragma once

#include <cstdint>
#include <vector>

#include "maia/core/memory_common.h"
#include "maia/mmem/mmem.h"

namespace maia {

using MemoryRegion = mmem::SegmentDescriptor;

class IProcess {
 public:
  virtual ~IProcess() = default;

  /// \brief Reads memory from one or more virtual addresses in a batch
  /// operation.
  ///
  /// \param addresses Span of virtual addresses to read from.
  /// \param bytes_per_address Number of bytes to read from each address.
  /// \param out_buffer Output span to write the data into. Must be at least
  ///                   addresses.size() * bytes_per_address bytes.
  /// \return true if all reads were successful, false otherwise.
  /// \note This enables platform-optimized batch operations
  ///       (e.g., process_vm_readv on Linux). For single address reads,
  ///       pass a span containing one address.
  virtual bool ReadMemory(std::span<const MemoryAddress> addresses,
                          size_t bytes_per_address,
                          std::span<std::byte> out_buffer) = 0;

  /// \brief Writes a block of memory to the process.
  /// \param address The base address to write to.
  /// \param buffer A span representing the buffer containing the data to write.
  /// \return true if the write was successful, false otherwise.
  virtual bool WriteMemory(uintptr_t address,
                           std::span<const std::byte> buffer) = 0;

  /// \brief Retrieves a list of all relevant memory regions in the process.
  /// \return A std::vector of MemoryRegion structs.
  virtual std::vector<MemoryRegion> GetMemoryRegions() const = 0;

  /// \brief Gets the process's unique identifier.
  /// \return The Process ID (PID).
  virtual uint32_t GetProcessId() const = 0;

  /// \brief Gets the process's name (e.g., "my_game.exe").
  /// \return The process name as a std::string.
  virtual std::string GetProcessName() const = 0;

  /// \brief Checks if the process handle is still valid and the process is
  /// running.
  /// \return true if the process is still accessible, false otherwise.
  virtual bool IsProcessValid() const = 0;

  /// \brief Gets the base address of the process's main executable module.
  /// \details This is the virtual memory address where the primary module
  /// (e.g., the .exe on Windows or the main ELF binary on Linux) is loaded into
  /// memory. This address is often randomized by ASLR.
  /// \return The base address of the main module, or 0 if it cannot be found.
  virtual uintptr_t GetBaseAddress() const = 0;
};

}  // namespace maia
