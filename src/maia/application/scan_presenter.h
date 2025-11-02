// Copyright (c) Maia

#pragma once

#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/gui/widgets/scanner_widget.h"

namespace maia::gui {

class ScanPresenter {
 public:
  ScanPresenter(ScanResultModel& scan_result_model,
                ProcessModel& process_model,
                ScannerWidget& scanner_widget)
      : scan_result_model_(scan_result_model),
        process_model_(process_model),
        scanner_widget_(scanner_widget) {
    // clang-format off
    sinks_.Connect<&ScanResultModel::FirstScan>(scanner_widget.signals.scan_button_pressed, scan_result_model)
          .Connect<&ScannerWidget::SetMemory>(scan_result_model.signals().memory_changed, scanner_widget);
    // clang-format on
  }

 private:
  void OnScanPressed() {
    LogInfo("Scan pressed.");
  }

  ScanResultModel& scan_result_model_;
  ProcessModel& process_model_;
  ScannerWidget& scanner_widget_;

  SinkStorage sinks_;
};

}  // namespace maia::gui
