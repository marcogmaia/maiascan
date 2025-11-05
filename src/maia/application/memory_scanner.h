// Copyright (c) Maia

#pragma once

#include <algorithm>
#include <ranges>

#include "maia/core/i_memory_scanner.h"
#include "maia/core/i_process.h"
#include "maia/logging.h"

namespace maia {

class MemoryScanner : public IMemoryScanner {
 public:
  explicit MemoryScanner(IProcess& process)
      : process_(process),
        memory_regions_(process_.GetMemoryRegions()) {}

  std::vector<uintptr_t> FirstScan(std::span<const std::byte> value) override {
    if (!process_.IsProcessValid()) {
      LogWarning("Process is invalid.");
      return {};
    }

    const auto value_finder = [this, value](MemoryRegion reg) {
      return FindValuesInRegion(reg, value);
    };
    auto view = memory_regions_ | std::views::transform(value_finder);
    return std::views::join(view) | std::ranges::to<std::vector<uintptr_t>>();
  }

 private:
  std::vector<uintptr_t> FindValuesInRegion(MemoryRegion region,
                                            std::span<const std::byte> value) {
    std::vector<std::byte> region_memory(region.size);
    if (value.empty() ||
        !process_.ReadMemory(region.base_address, region_memory)) {
      return {};
    }

    constexpr size_t kPageSize = 4096;
    std::vector<uintptr_t> addresses_found;
    addresses_found.reserve(kPageSize);

    size_t offset = 0;
    auto it = region_memory.begin();
    while (true) {
      it = std::search(it, region_memory.end(), value.begin(), value.end());
      if (it >= region_memory.end()) {
        break;
      }

      offset = std::distance(region_memory.begin(), it);
      addresses_found.emplace_back(region.base_address + offset);
      std::advance(it, value.size());
    }
    return addresses_found;
  }

  IProcess& process_;

  std::vector<MemoryRegion> memory_regions_;
};

}  // namespace maia
