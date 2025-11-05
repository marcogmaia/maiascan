// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include "memory_scanner.h"

namespace maia {

void ScanResultModel::ScanForValue(std::vector<std::byte> value_to_scan) {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    LogWarning("No process is currently active.");
    memory_scanner_.reset();
    return;
  }
  LogInfo("Scan pressed.");

  const auto scanned_values = memory_scanner_->FirstScan(value_to_scan);
  for (const auto& value : scanned_values) {
    std::scoped_lock guard(mutex_);
    std::vector<std::byte> bytes(4);
    active_process_->ReadMemory(value, bytes);
    entries_.emplace_back(ScanEntry{.address = value, .data = bytes});
  }
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
            for (auto& entry : entries_) {
              active_process_->ReadMemory(entry.address, entry.data);
            }
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
      }) {}

}  // namespace maia
