// Copyright (c) Maia

#pragma once

#include <vector>

#include "maia/scanner/i_process_memory_accessor.h"
#include "maia/scanner/memory_common.h"

namespace maia::scanner {

ProcessHandle OpenHandle(uint32_t pid);

// This class is platform-specific (e.g., uses ReadProcessMemory on Windows)
// It's the ONLY part that's hard to test.
class LiveProcessAccessor : public IProcessMemoryAccessor {
 public:
  explicit LiveProcessAccessor(ProcessHandle handle);

  std::vector<MemoryRegion> GetMemoryRegions() override;

  bool ReadMemory(MemoryPtr address, std::span<std::byte> buffer) override;

  bool WriteMemory(MemoryPtr address, std::span<const std::byte> data) override;

 private:
  ProcessHandle handle_;
  MemoryAddress process_base_address_;
};

}  // namespace maia::scanner
