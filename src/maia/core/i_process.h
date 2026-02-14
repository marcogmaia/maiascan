// Copyright (c) Maia

/// \file i_process.h
/// \brief Interface abstraction for a target process.
///
/// \details
/// **Role**: Defines the contract for interacting with a target process,
/// decoupling core business logic (like scanning) from OS-specific
/// implementations.
///
/// **Architecture**:
///    - **Strategy Pattern**: Allows swapping the underlying process access
///    mechanism
///      (e.g., local Windows process, remote network process, mock for testing)
///      without changing the scanning logic.
///
/// **Thread Safety**:
///    - Implementations must ensure that `ReadMemory` and constant getters are
///    thread-safe.
///    - State-modifying methods (`Suspend`, `Resume`, `WriteMemory`) should be
///    synchronized by the caller if needed.
///
/// **Key Interactions**:
///    - Implemented by `maia::Process` (Standard) and `maia::FakeProcess`
///    (Tests).
///    - Consumed by `Scanner`, `CheatTableModel`, and `PointerScannerModel`.

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
  /// \param success_mask (Optional) A mask to store success/failure per address
  /// (1 for success, 0 for failure).
  ///                     If provided, the function returns true even on partial
  ///                     failure.
  /// \return true if all reads were successful (or partial success if mask is
  /// provided), false otherwise.
  /// \note This enables platform-optimized batch operations
  virtual bool ReadMemory(std::span<const MemoryAddress> addresses,
                          size_t bytes_per_address,
                          std::span<std::byte> out_buffer,
                          std::vector<uint8_t>* success_mask) = 0;

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

  /// \brief Retrieves a list of all loaded modules in the process.
  /// \return A std::vector of ModuleDescriptor structs.
  virtual std::vector<mmem::ModuleDescriptor> GetModules() const = 0;

  /// \brief Suspends all threads in the process.
  /// \return true if successful, false otherwise.
  virtual bool Suspend() = 0;

  /// \brief Resumes all threads in the process.
  /// \return true if successful, false otherwise.
  virtual bool Resume() = 0;

  /// \brief Gets the pointer size of the process (4 for 32-bit, 8 for 64-bit).
  /// \return The pointer size in bytes.
  virtual size_t GetPointerSize() const = 0;
};

}  // namespace maia
