// Copyright (c) Maia

#pragma once

#include <vector>

#include "core/i_process.h"
#include "maia/core/memory_common.h"

namespace maia::scanner {

ProcessHandle OpenHandle(uint32_t pid);

class LiveProcessAccessor : public IProcess {
 public:
  explicit LiveProcessAccessor(ProcessHandle handle);

  bool ReadMemory(uintptr_t address,
                  std::span<std::byte> buffer) const override;

  bool WriteMemory(uintptr_t address,
                   std::span<const std::byte> buffer) override;

  std::vector<MemoryRegion> GetMemoryRegions() const override;

  uint32_t GetProcessId() const override;

  std::string GetProcessName() const override;

  bool IsProcessValid() const override;

  uintptr_t GetBaseAddress() const override;

 private:
  ProcessHandle handle_;
  uint32_t process_id_;
  MemoryAddress process_base_address_;
};

}  // namespace maia::scanner
