// Copyright (c) Maia

#pragma once

#include <optional>
#include <string_view>

#include "maia/core/i_process.h"
#include "maia/mmem/mmem.h"

namespace maia {

/// \brief An implementation of the IProcess interface using the mmem library.
///
/// This class acts as an adapter, mapping the object-oriented IProcess
/// calls to the C-style free functions provided by the mmem namespace.
/// It holds a ProcessDescriptor to operate on a specific target process.
class Process : public IProcess {
 public:
  /// \brief Attempts to create an MmemProcess instance by process ID.
  /// \param pid The process identifier.
  /// \return An MmemProcess wrapped in std::optional, or std::nullopt if the
  ///         process is not found or inaccessible.
  static std::optional<Process> Create(uint32_t pid);

  /// \brief Attempts to create an MmemProcess instance by process name.
  /// \param name The executable name (e.g., "my_game.exe").
  /// \return An MmemProcess wrapped in std::optional, or std::nullopt if the
  ///         process is not found.
  static std::optional<Process> Create(std::string_view name);

  // Delete default and copy constructors
  Process() = delete;
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;

  // Enable move semantics
  Process(Process&&) = default;
  Process& operator=(Process&&) = default;

  /// \brief Reads memory from one or more virtual addresses.
  bool ReadMemory(std::span<const MemoryAddress> addresses,
                  size_t bytes_per_address,
                  std::span<std::byte> out_buffer,
                  std::vector<uint8_t>* success_mask = nullptr) override;

  /// \brief Writes a block of memory to the process.

  bool WriteMemory(uintptr_t address,
                   std::span<const std::byte> buffer) override;

  /// \brief Retrieves a list of all relevant memory regions (segments).
  /// \note This implementation assumes maia::MemoryRegion (from
  ///       memory_common.h) is structurally compatible with
  ///       maia::mmem::SegmentDescriptor (base, size) and that its
  ///       protection field is compatible with a uint32_t.
  std::vector<MemoryRegion> GetMemoryRegions() const override;

  /// \brief Gets the process's unique identifier (PID).
  uint32_t GetProcessId() const override;

  /// \brief Gets the process's name (e.g., "my_game.exe").
  std::string GetProcessName() const override;

  /// \brief Checks if the process is still running.
  bool IsProcessValid() const override;

  /// \brief Gets the base address of the process's main executable module.
  uintptr_t GetBaseAddress() const override;

  /// \brief Suspends all threads in the process.
  bool Suspend() override;

  /// \brief Resumes all threads in the process.
  bool Resume() override;

 private:
  /// \brief Private constructor for use by the static factory methods.
  explicit Process(mmem::ProcessDescriptor descriptor) noexcept;

  mmem::ProcessDescriptor descriptor_;
};

}  // namespace maia
