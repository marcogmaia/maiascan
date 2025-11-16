// Copyright (c) Maia

#include "maia/application/scanner_presenter.h"

namespace maia {

ScannerPresenter::ScannerPresenter(ScanResultModel& scan_result_model,
                                   ProcessModel& process_model,
                                   ScannerWidget& scanner_widget)
    : scan_result_model_(scan_result_model),
      process_model_(process_model),
      scanner_widget_(scanner_widget) {
  // clang-format off
  sinks_
  .Connect<&ScanResultModel::FirstScan>(scanner_widget_.signals().new_scan_pressed, scan_result_model_)
  // .Connect<&ScanResultModel::FilterChangedValues>(scanner_widget.signals().filter_changed, scan_result_model_)
        // .Connect<&ScanResultModel::ScanForValue>(scanner_widget_.signals().scan_button_pressed, scan_result_model_)
        .Connect<&ScanResultModel::SetActiveProcess>(process_model_.signals().active_process_changed, scan_result_model_)
        ;
  // clang-format on
}

}  // namespace maia
