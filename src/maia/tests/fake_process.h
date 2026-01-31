// Copyright (c) Maia

#pragma once

#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "maia/core/i_process.h"

namespace maia::test {

class FakeProcess : public IProcess {
 public:
  explicit FakeProcess(size_t memory_size = 0x4000);

  template <typename T>
  void WriteValue(size_t offset, T value) {
    static_assert(std::is_trivially_copyable_v<T>);
    WriteRawMemory(offset, std::as_bytes(std::span{&value, 1}));
  }

  void MarkAddressInvalid(uintptr_t addr);

  std::vector<std::byte>& GetRawMemory();

  bool ReadMemory(std::span<const MemoryAddress> addresses,
                  size_t bytes_per_address,
                  std::span<std::byte> out_buffer,
                  std::vector<uint8_t>* success_mask) override;

  bool WriteMemory(uintptr_t address,
                   std::span<const std::byte> buffer) override;

  std::vector<MemoryRegion> GetMemoryRegions() const override;

  void AddModule(std::string name, uintptr_t base, size_t size);

  std::vector<mmem::ModuleDescriptor> GetModules() const override;

  uint32_t GetProcessId() const override;

  std::string GetProcessName() const override;

  bool IsProcessValid() const override;

  uintptr_t GetBaseAddress() const override;

  bool Suspend() override;

  bool Resume() override;

  void SetValid(bool valid);

 private:
  void WriteRawMemory(size_t offset, std::span<const std::byte> data);

  std::vector<std::byte> memory_;
  std::vector<mmem::ModuleDescriptor> modules_;
  uintptr_t base_address_;
  bool is_valid_ = true;
  std::unordered_set<uintptr_t> invalid_addresses_;
};

}  // namespace maia::test
