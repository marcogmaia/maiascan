// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <ranges>

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
  LogInfo("Scan pressed.");

  Clear();
  const auto scanned_values = memory_scanner_->FirstScan(value_to_scan);
  for (const auto& value : scanned_values) {
    std::scoped_lock guard(mutex_);
    std::vector<std::byte> bytes(4);
    active_process_->ReadMemory(value, bytes);
    entries_.emplace_back(ScanEntry{.address = value, .data = bytes});
  }
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
