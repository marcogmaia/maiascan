// Copyright (c) Maia

#include <memory>
#include <span>

#include "maia/application/memory_scanner.h"
#include "maia/application/scanner_presenter.h"

namespace maia {

ScannerPresenter::ScannerPresenter(ScanResultModel& scan_result_model,
                                   ProcessModel& process_model,
                                   ScannerWidget& scanner_widget)
    : scan_result_model_(scan_result_model),
      process_model_(process_model),
      scanner_widget_(scanner_widget) {
  // clang-format off
    sinks_.Connect<&ScannerPresenter::OnScanPressed>(scanner_widget_.signals().scan_button_pressed, *this)
          .Connect<&ScannerPresenter::SetActiveProcess>(process_model_.signals().active_process_changed, *this);
  // clang-format on
}

void ScannerPresenter::OnScanPressed() {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    LogWarning("No process is currently active.");
    memory_scanner_.reset();
    return;
  }
  LogInfo("Scan pressed.");
  // memory_scanner_
  uint32_t needle = 1337;
  auto svals = std::span(reinterpret_cast<std::byte*>(&needle), sizeof(needle));
  auto vals = memory_scanner_->FirstScan(svals);
  for (auto& val : vals) {
    std::vector<std::byte> bytes(4);
    active_process_->ReadMemory(val, bytes);
    scan_entries_.emplace_back(ScanEntry{.address = val, .data = bytes});
  }
}

void ScannerPresenter::SetActiveProcess(IProcess* process) {
  LogInfo("Scanner changed active process: {}, PID: {}",
          process->GetProcessName(),
          process->GetProcessId());
  active_process_ = process;
  memory_scanner_ = std::make_unique<MemoryScanner>(*active_process_);
}

}  // namespace maia
