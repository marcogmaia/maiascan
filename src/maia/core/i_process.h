// Copyright (c) Maia

#pragma once

#include <cstdint>
#include <vector>

#include "maia/core/memory_common.h"

namespace maia {

class IProcess {
 public:
  virtual ~IProcess() = default;

  /// \brief Reads a block of memory into the provided buffer.
  /// \param address The base address to read from.
  /// \param buffer A span representing the caller-allocated buffer.
  /// \return true if the read was successful, false otherwise.
  virtual bool ReadMemory(uintptr_t address,
                          std::span<std::byte> buffer) const = 0;

  /// \brief Writes a block of memory to the process.
  /// \param address The base address to write to.
  /// \param buffer A span representing the buffer containing the data to write.
  /// \return true if the write was successful, false otherwise.
  virtual bool WriteMemory(std::uintptr_t address,
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
};

}  // namespace maia
