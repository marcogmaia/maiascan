// Copyright (c) Maia

#include "memory_scanner.h"

#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>
#include <vector>

#include "maia/core/scan_result.h"
#include "maia/logging.h"

namespace maia {

namespace {

std::vector<uintptr_t> FindValuesInRegion(IProcess& process,
                                          MemoryRegion region,
                                          std::span<const std::byte> value) {
  std::vector<std::byte> region_memory(region.size);
  if (value.empty() ||
      !process.ReadMemory(region.base_address, region_memory)) {
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

template <CScannableType T>
std::vector<std::byte> ToBytes(const ScanParams& scan) {
  ScanParamsType<T> sp = std::get<ScanParamsType<T>>(scan);
  auto s = std::span(reinterpret_cast<std::byte*>(&sp.value), sizeof(T));
  std::vector<std::byte> vec;
  vec.assign(s.begin(), s.end());
  return vec;
}

template <CScannableType T>
void UpdateSnapshotValues(IProcess& process, MemorySnapshot& snapshot) {
  snapshot.values.clear();
  snapshot.values.reserve(snapshot.values.size() * sizeof(T));

  for (const auto& addr : snapshot.addresses) {
    auto buffer = ReadCurrent<T>(process, addr);
    std::span<std::byte, sizeof(T)> buffer_span(
        reinterpret_cast<std::byte*>(&buffer), sizeof(T));
    snapshot.values.append_range(buffer_span);
  }
}

template <CScannableType T>
std::shared_ptr<MemorySnapshot> ScanRegions(
    std::span<const MemoryRegion> regions,
    IProcess& process,
    const ScanParams& params) {
  auto snapshot = std::make_shared<MemorySnapshot>();
  auto bytes = ToBytes<T>(params);
  const auto value_finder = [&process, bytes](MemoryRegion reg) {
    return FindValuesInRegion(process, reg, bytes);
  };
  auto view = regions | std::views::transform(value_finder);
  snapshot->addresses =
      std::views::join(view) | std::ranges::to<std::vector<uintptr_t>>();
  UpdateSnapshotValues<T>(process, *snapshot);
  return snapshot;
}

}  // namespace

ScanResult MemoryScanner::NewScan(const ScanParams& params) {
  if (!process_.IsProcessValid()) {
    LogWarning("Process is not valid.");
    return {};
  }

  snapshot_ = ScanRegions<uint32_t>(memory_regions_, process_, params);
  return ScanResult::FromSnapshot<uint32_t>(snapshot_);
}

ScanResult MemoryScanner::NextScan(const ScanResult& previous_result,
                                   const ScanParams& params) {
  // TODO: Implement this pure virtual method.
  return {};
}

}  // namespace maia
