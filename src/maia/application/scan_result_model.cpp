// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <algorithm>
#include <memory>
#include <ranges>

#include "logging.h"
#include "memory_scanner.h"

namespace maia {

namespace {

void UpdateScannedValues(IProcess& process, std::vector<ScanEntry>& entries) {
  for (auto& entry : entries) {
    process.ReadMemory(entry.address, entry.data);
  }
}

}  // namespace

void ScanResultModel::ScanForValue(std::vector<std::byte> value_to_scan) {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    LogWarning("No process is currently active.");
    memory_scanner_.reset();
    return;
  }
  LogInfo("Scan pressed for value with {} bytes", value_to_scan.size());

  Clear();

  // Determine the type based on the byte vector size
  if (value_to_scan.empty()) {
    LogWarning("Empty value to scan");
    return;
  }

  // Get all memory regions from the process
  auto regions = active_process_->GetMemoryRegions();
  LogInfo("Found {} memory regions to scan", regions.size());

  std::scoped_lock guard(mutex_);

  // Scan each memory region for the value
  for (const auto& region : regions) {
    // if (!region.is_readable || region.size == 0) {
    //   continue;
    // }

    // Read the entire region
    std::vector<std::byte> region_memory(region.size);
    if (!active_process_->ReadMemory(region.base_address, region_memory)) {
      continue;
    }

    // Search for the value pattern in this region
    auto it = region_memory.begin();
    while (true) {
      it = std::search(
          it, region_memory.end(), value_to_scan.begin(), value_to_scan.end());

      if (it >= region_memory.end()) {
        break;
      }

      size_t offset = std::distance(region_memory.begin(), it);
      uintptr_t found_address = region.base_address + offset;

      // Read the actual value at this address for display
      std::vector<std::byte> actual_value(value_to_scan.size());
      if (active_process_->ReadMemory(found_address, actual_value)) {
        entries_.emplace_back(
            ScanEntry{.address = found_address, .data = actual_value});
      }

      // Move past this match to find next one
      std::advance(it, value_to_scan.size());
    }
  }

  LogInfo("Scan completed, found {} addresses", entries_.size());
  prev_entries_ = entries_;
}

void ScanResultModel::SetActiveProcess(IProcess* process) {
  LogInfo("Scanner changed active process: {}, PID: {}",
          process->GetProcessName(),
          process->GetProcessId());
  active_process_ = process;
  memory_scanner_ = std::make_unique<MemoryScanner>(*active_process_);
}

ScanResultModel::ScanResultModel()
    : task_([this](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
          if (active_process_ && active_process_->IsProcessValid()) {
            std::scoped_lock guard(mutex_);
            UpdateScannedValues(*active_process_, entries_);
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
      }) {}

void ScanResultModel::FilterChangedValues() {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    return;
  }

  std::scoped_lock aguard(mutex_);
  thread_local std::vector<ScanEntry> new_entries;
  new_entries.clear();
  new_entries.reserve(entries_.size());
  UpdateScannedValues(*active_process_, entries_);
  for (const auto& [prev, curr] : std::views::zip(prev_entries_, entries_)) {
    if (prev.data != curr.data) {
      new_entries.emplace_back(curr);
    }
  };
  entries_.swap(new_entries);
  prev_entries_ = entries_;
}

void ScanResultModel::FirstScan(std::vector<std::byte> value_to_scan) {
  Clear();
  ScanForValue(value_to_scan);
}

void ScanResultModel::Clear() {
  std::scoped_lock guard(mutex_);
  prev_entries_.clear();
  entries_.clear();
}

}  // namespace maia
