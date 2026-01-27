// Copyright (c) Maia

#include "maia/tests/fake_process.h"

#include <algorithm>
#include <ranges>

#include "maia/assert.h"

namespace maia::test {

FakeProcess::FakeProcess(size_t memory_size) {
  memory_.resize(memory_size, std::byte{0});
  base_address_ = 0x100000;
}

void FakeProcess::MarkAddressInvalid(uintptr_t addr) {
  invalid_addresses_.insert(addr);
}

std::vector<std::byte>& FakeProcess::GetRawMemory() {
  return memory_;
}

bool FakeProcess::ReadMemory(std::span<const MemoryAddress> addresses,
                             size_t bytes_per_address,
                             std::span<std::byte> out_buffer,
                             std::vector<uint8_t>* success_mask) {
  if (!is_valid_) {
    return false;
  }

  if (out_buffer.size() < addresses.size() * bytes_per_address) {
    return false;
  }

  bool all_success = true;
  auto chunks = out_buffer | std::views::chunk(bytes_per_address);

  for (auto [i, pair] :
       std::views::enumerate(std::views::zip(addresses, chunks))) {
    const auto [addr, chunk] = pair;
    bool success = true;

    const size_t offset = addr - base_address_;
    const bool is_invalid = invalid_addresses_.contains(addr) ||
                            addr < base_address_ ||
                            offset + bytes_per_address > memory_.size();

    if (is_invalid) {
      success = false;
      std::ranges::fill(chunk, std::byte{0});
    } else {
      std::ranges::copy(std::span{memory_}.subspan(offset, bytes_per_address),
                        chunk.begin());
    }

    if (success_mask && i < success_mask->size()) {
      (*success_mask)[i] = success ? 1 : 0;
    }
    all_success = all_success && success;
  }

  if (success_mask) {
    return true;
  }

  return all_success;
}

bool FakeProcess::WriteMemory(uintptr_t address,
                              std::span<const std::byte> buffer) {
  if (!is_valid_) {
    return false;
  }

  const size_t offset = address - base_address_;
  if (address < base_address_ || offset + buffer.size() > memory_.size() ||
      invalid_addresses_.contains(address)) {
    return false;
  }

  std::ranges::copy(buffer, memory_.begin() + offset);
  return true;
}

std::vector<MemoryRegion> FakeProcess::GetMemoryRegions() const {
  if (!is_valid_) {
    return {};
  }
  MemoryRegion region;
  region.base = base_address_;
  region.size = memory_.size();
  region.protection = mmem::Protection::kReadWrite;
  return {region};
}

std::vector<mmem::ModuleDescriptor> FakeProcess::GetModules() const {
  return {};
}

uint32_t FakeProcess::GetProcessId() const {
  return 1234;
}

std::string FakeProcess::GetProcessName() const {
  return "test_app.exe";
}

bool FakeProcess::IsProcessValid() const {
  return is_valid_;
}

uintptr_t FakeProcess::GetBaseAddress() const {
  return base_address_;
}

bool FakeProcess::Suspend() {
  return true;
}

bool FakeProcess::Resume() {
  return true;
}

void FakeProcess::SetValid(bool valid) {
  is_valid_ = valid;
}

void FakeProcess::WriteRawMemory(size_t offset,
                                 std::span<const std::byte> data) {
  Assert((offset + data.size()) <= memory_.size());
  std::ranges::copy(data, memory_.begin() + offset);
}

}  // namespace maia::test
