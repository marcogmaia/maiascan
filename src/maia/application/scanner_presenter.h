// Copyright (c) Maia

#pragma once

#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/core/signals.h"
#include "maia/gui/widgets/scanner_widget.h"

namespace maia {

class ScannerPresenter {
 public:
  ScannerPresenter(ScanResultModel& scan_result_model,
                   ProcessModel& process_model,
                   ScannerWidget& scanner_widget);

  void Render() {
    scanner_widget_.Render(scan_entries_);
  }

 private:
  void OnScanPressed();

  void SetActiveProcess(IProcess* process);

  ScanResultModel& scan_result_model_;
  ProcessModel& process_model_;
  ScannerWidget& scanner_widget_;

  IProcess* active_process_{};

  std::vector<ScanEntry> scan_entries_;

  std::unique_ptr<IMemoryScanner> memory_scanner_;

  SinkStorage sinks_;
};

}  // namespace maia
