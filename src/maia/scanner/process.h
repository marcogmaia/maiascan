// Copyright (c) Maia

#pragma once

#include <vector>

#include "maia/scanner/i_process_memory_accessor.h"
#include "maia/scanner/memory_common.h"

namespace maia::scanner {

// This class is platform-specific (e.g., uses ReadProcessMemory on Windows)
// It's the ONLY part that's hard to test.
class LiveProcessAccessor : public IProcessMemoryAccessor {
 public:
  using ProcessHandle = MemoryPtr;

  explicit LiveProcessAccessor(ProcessHandle handle)
      : handle_(handle) {}

  std::vector<MemoryRegion> GetMemoryRegions() override;

  bool ReadMemory(MemoryPtr address, std::span<std::byte> buffer) override;

  bool WriteMemory(MemoryPtr address, std::span<const std::byte> data) override;

 private:
  ProcessHandle handle_;
};

}  // namespace maia::scanner
