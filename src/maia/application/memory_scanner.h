// Copyright (c) Maia

#pragma once

#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>
#include <vector>

#include "maia/core/i_memory_scanner.h"
#include "maia/core/i_process.h"
#include "maia/core/scan_types.h"
#include "maia/logging.h"

namespace maia {

// // Safely converts a byte span to uint32_t (handles unaligned data).
// inline uint32_t BytesToU32(std::span<const std::byte> bytes) {
//   if (bytes.size() < sizeof(uint32_t)) {
//     return 0;
//   }
//   uint32_t value;
//   std::memcpy(&value, bytes.data(), sizeof(value));
//   return value;
// }

// uint32_t ToUin32t(std::span<const std::byte> data) {
//   uint32_t value;
//   constexpr size_t kSize = sizeof(value);
//   if (data.size_bytes() < kSize) {
//     LogCritical("Invalid span.");
//   }
//   std::memcpy(&value, data.data(), kSize);
//   return value;
// }

// MemoryScanner implementation for kU32 (32-bit unsigned integer) scans.
class MemoryScanner : public IMemoryScanner {
 public:
  explicit MemoryScanner(IProcess& process)
      : process_(process),
        memory_regions_(process_.GetMemoryRegions()) {}

  // TODO: Fix this
  ScanResult NewScan(const ScanParams& params) override {
    if (!process_.IsProcessValid()) {
      LogWarning("Process is invalid.");
      return {};
    }

    const auto value_finder = [this, params](MemoryRegion reg) {
      auto res = std::get<ScanParamsTyped<uint32_t>>(params);
      return FindValuesInRegion(
          reg, std::span(reinterpret_cast<std::byte*>(&res.value), 4));
    };
    auto view = memory_regions_ | std::views::transform(value_finder);
    auto addresses =
        std::views::join(view) | std::ranges::to<std::vector<uintptr_t>>();

    // MakeScanParams(ScanComparison::kExactValue, ToUin32t(params.));

    return FixedScanResult<uint32_t>{.addresses = std::move(addresses)};
  }

  // std::get<ScanParamsTyped<ScanValueType::kU32>>()

  ScanResult NextScan(const ScanResult& previous_result,
                      const ScanParams& params) override {
    // TODO: Implement this pure virtual method.
    return {};
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
  // TODO: Save a snapshot of the memory.
};

}  // namespace maia
