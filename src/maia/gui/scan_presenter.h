// Copyright (c) Maia

#pragma once

#include "application/scan_result_model.h"
#include "widgets/scanner_widget.h"

namespace maia::gui {

class ScanPresenter {
 public:
  ScanPresenter(ScanResultModel& model, ScannerWidget& widget) {
    // clang-format off
    sinks_.Connect<&ScanResultModel::FirstScan>(widget.signals.scan_button_pressed, model)
          .Connect<&ScannerWidget::SetMemory>(model.signals().memory_changed, widget);
    // clang-format on
  }

 private:
  void OnScanPressed() {
    LogInfo("Scan pressed.");
  }

  SinkStorage sinks_;
};

}  // namespace maia::gui
