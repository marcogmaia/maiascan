// Copyright (c) Maia

#include "memory_scanner.h"

#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>
#include <vector>

#include "maia/core/memory_io.h"
#include "maia/core/scan_result.h"
#include "maia/logging.h"

namespace maia {

namespace {

template <CScannableType T>
std::vector<std::byte> ToBytes(const ScanParamsType<T>& params) {
  auto s =
      std::span(reinterpret_cast<const std::byte*>(&params.value), sizeof(T));
  return std::vector<std::byte>(s.begin(), s.end());
}

template <CScannableType T>
void UpdateSnapshotValues(IProcess& process, MemorySnapshot& snapshot) {
  snapshot.values.clear();
  snapshot.values.reserve(snapshot.addresses.size() * sizeof(T));

  for (const auto& addr : snapshot.addresses) {
    auto buffer = ReadAt<T>(process, addr);
    std::span<std::byte, sizeof(T)> buffer_span(
        reinterpret_cast<std::byte*>(&buffer), sizeof(T));
    snapshot.values.insert(
        snapshot.values.end(), buffer_span.begin(), buffer_span.end());
  }
}

template <CScannableType T>
bool CompareValue(const T& current,
                  const T& target,
                  ScanComparison comparison) {
  switch (comparison) {
    case ScanComparison::kExactValue:
      return current == target;
    case ScanComparison::kNotEqual:
      return current != target;
    case ScanComparison::kGreaterThan:
      return current > target;
    case ScanComparison::kLessThan:
      return current < target;
    case ScanComparison::kBetween:
      return current >= target &&
             current <= target;  // Note: upper_bound should be in params
    case ScanComparison::kNotBetween:
      return current < target ||
             current > target;  // Note: upper_bound should be in params
    default:
      return false;
  }
}

template <CScannableType T>
std::vector<uintptr_t> FindValuesInRegion(IProcess& process,
                                          MemoryRegion region,
                                          const ScanParamsType<T>& params) {
  std::vector<std::byte> region_memory(region.size);
  if (!process.ReadMemory(region.base_address, region_memory)) {
    return {};
  }

  std::vector<uintptr_t> addresses_found;
  addresses_found.reserve(256);  // Reserve reasonable initial capacity

  const size_t value_size = sizeof(T);
  const auto bytes = ToBytes(params);

  // For exact value matching, use std::search for byte pattern matching
  if (params.comparison == ScanComparison::kExactValue) {
    auto it = region_memory.begin();
    while (true) {
      it = std::search(it, region_memory.end(), bytes.begin(), bytes.end());
      if (it >= region_memory.end()) {
        break;
      }

      size_t offset = std::distance(region_memory.begin(), it);
      addresses_found.emplace_back(region.base_address + offset);
      std::advance(it, value_size);
    }
  } else {
    // For other comparisons, check each properly aligned position
    // Align to type size boundary for proper value reading
    for (size_t offset = 0; offset + value_size <= region_memory.size();
         offset += value_size) {
      T current_value;
      std::memcpy(&current_value, &region_memory[offset], value_size);

      if (CompareValue(current_value, params.value, params.comparison)) {
        addresses_found.emplace_back(region.base_address + offset);
      }
    }
  }

  return addresses_found;
}

template <CScannableType T>
std::shared_ptr<MemorySnapshot> ScanRegions(
    std::span<const MemoryRegion> regions,
    IProcess& process,
    const ScanParamsType<T>& params) {
  auto snapshot = std::make_shared<MemorySnapshot>();

  const auto value_finder = [&process, &params](MemoryRegion reg) {
    return FindValuesInRegion(process, reg, params);
  };

  auto view = regions | std::views::transform(value_finder);
  snapshot->addresses =
      std::views::join(view) | std::ranges::to<std::vector<uintptr_t>>();

  UpdateSnapshotValues<T>(process, *snapshot);
  return snapshot;
}

template <CScannableType T>
std::shared_ptr<MemorySnapshot> NextScanRegions(
    const ScanResult& previous_result,
    IProcess& process,
    const ScanParamsType<T>& params) {
  auto snapshot = std::make_shared<MemorySnapshot>();

  // For subsequent scans, we only check the addresses from the previous result
  const auto& addresses = previous_result.addresses();

  for (uintptr_t addr : addresses) {
    T current_value = ReadAt<T>(process, addr);

    bool should_include = false;
    switch (params.comparison) {
      case ScanComparison::kChanged: {
        // Compare with previous value
        auto prev_values = previous_result.values<T>();
        auto it = std::ranges::find(addresses, addr);
        if (it != addresses.end()) {
          size_t index = std::distance(addresses.begin(), it);
          auto prev_value_it = prev_values.begin();
          std::advance(prev_value_it, index);
          if (prev_value_it != prev_values.end()) {
            should_include = current_value != *prev_value_it;
          }
        }
        break;
      }
      case ScanComparison::kUnchanged:
        // Similar logic for unchanged
        should_include = current_value == params.value;  // Simplified
        break;
      case ScanComparison::kIncreased:
        should_include = current_value > params.value;  // Simplified
        break;
      case ScanComparison::kDecreased:
        should_include = current_value < params.value;  // Simplified
        break;
      case ScanComparison::kIncreasedBy:
        should_include =
            (current_value - params.value) == params.value;  // Simplified
        break;
      case ScanComparison::kDecreasedBy:
        should_include =
            (params.value - current_value) == params.value;  // Simplified
        break;
      default:
        // For other comparisons, use the same logic as initial scan
        should_include =
            CompareValue(current_value, params.value, params.comparison);
        break;
    }

    if (should_include) {
      snapshot->addresses.push_back(addr);
      std::span<std::byte, sizeof(T)> value_span(
          reinterpret_cast<std::byte*>(&current_value), sizeof(T));
      snapshot->values.insert(
          snapshot->values.end(), value_span.begin(), value_span.end());
    }
  }

  return snapshot;
}

struct ScanVisitor {
  std::span<const MemoryRegion> regions;
  IProcess& process;
  std::shared_ptr<const MemorySnapshot>& snapshot;

  template <CScannableType T>
  ScanResult operator()(const ScanParamsType<T>& params) {
    auto new_snapshot = ScanRegions<T>(regions, process, params);
    snapshot = new_snapshot;
    return ScanResult::FromSnapshot<T>(snapshot);
  }

  ScanResult operator()(const auto& params) {
    // Handle variable-length types (string, wstring, vector<byte>) - not
    // implemented yet
    LogWarning("Variable-length scan types not yet implemented");
    return {};
  }
};

struct NextScanVisitor {
  const ScanResult& previous_result;
  IProcess& process;
  std::shared_ptr<const MemorySnapshot>& snapshot;

  template <CScannableType T>
  ScanResult operator()(const ScanParamsType<T>& params) {
    auto new_snapshot = NextScanRegions<T>(previous_result, process, params);
    snapshot = new_snapshot;
    return ScanResult::FromSnapshot<T>(snapshot);
  }

  ScanResult operator()(const auto& params) {
    LogWarning("Variable-length scan types not yet implemented");
    return {};
  }
};

}  // namespace

ScanResult MemoryScanner::NewScan(const ScanParams& params) {
  if (!process_.IsProcessValid()) {
    LogWarning("Process is not valid.");
    return {};
  }

  ScanVisitor visitor{
      .regions = memory_regions_, .process = process_, .snapshot = snapshot_};
  return std::visit(visitor, params);
}

ScanResult MemoryScanner::NextScan(const ScanResult& previous_result,
                                   const ScanParams& params) {
  if (!process_.IsProcessValid()) {
    LogWarning("Process is not valid.");
    return {};
  }

  NextScanVisitor visitor{previous_result, process_, snapshot_};
  return std::visit(visitor, params);
}

}  // namespace maia
