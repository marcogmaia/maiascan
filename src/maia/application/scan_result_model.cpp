// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <algorithm>
#include <span>

#include "maia/logging.h"
#include "maia/mmem/mmem.h"

namespace maia {

namespace {

constexpr bool IsReadable(mmem::Protection prot) noexcept {
  const auto prot_val = static_cast<uint32_t>(prot);
  const auto read_val = static_cast<uint32_t>(mmem::Protection::kRead);
  return (prot_val & read_val) != 0;
}

std::optional<std::vector<std::byte>> ReadRegion(const MemoryRegion& region,
                                                 IProcess& process) {
  std::vector<std::byte> buffer(region.size);
  if (!process.ReadMemory({&region.base, 1}, region.size, buffer)) {
    return std::nullopt;
  }
  return buffer;
}

bool CanScan(IProcess* process) {
  return process && process->IsProcessValid();
}

}  // namespace

void ScanResultModel::FirstScan(std::vector<std::byte> value_to_scan) {
  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid.");
    return;
  }
  // Lock the mutex for the entire operation
  std::scoped_lock lock(mutex_);

  prev_entries_ = std::move(entries_);
  entries_.clear();

  if (value_to_scan.empty()) {
    signals_.memory_changed.publish(entries_);
    return;
  }

  constexpr auto kPageSize = 4096;
  std::vector<ScanEntry> new_entries;
  new_entries.reserve(kPageSize);

  auto regions = active_process_->GetMemoryRegions();

  for (const auto& region : regions) {
    // Skip non-readable regions
    if (!IsReadable(region.protection)) {
      continue;
    }

    // Read the entire region.
    std::optional<std::vector<std::byte>> region_buffer =
        ReadRegion(region, *active_process_);
    if (!region_buffer) {
      continue;
    }

    // Search for the value pattern in this region.
    auto& read_buffer = *region_buffer;
    auto it = read_buffer.begin();
    auto end = read_buffer.end();
    while (true) {
      it = std::search(it, end, value_to_scan.begin(), value_to_scan.end());
      if (it == end) {
        break;  // Not found
      }

      size_t offset = std::distance(read_buffer.begin(), it);
      new_entries.emplace_back(region.base + offset, value_to_scan);

      // Move past this match to find the next one.
      std::advance(it, value_to_scan.size());
    }
  }

  entries_ = std::move(new_entries);
  signals_.memory_changed.publish(entries_);
}

void ScanResultModel::SetActiveProcess(IProcess* process) {
  active_process_ = process;
}

void ScanResultModel::Clear() {
  entries_.clear();
  prev_entries_.clear();
}

}  // namespace maia
