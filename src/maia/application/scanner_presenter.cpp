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

  connections_.emplace_back(scanner_widget_.sinks().NewScanPressed().connect<&ScanResultModel::FirstScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().NextScanPressed().connect<&ScanResultModel::NextScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().ScanComparisonSelected().connect<&ScanResultModel::SetScanComparison>(scan_result_model_));

  connections_.emplace_back(process_model_.sinks().ActiveProcessChanged().connect<&ScanResultModel::SetActiveProcess>(scan_result_model_));
  // clang-format on

  // TODO: Set initial state.
  // scanner_widget_.signals().scan_comparison_selected.publish(
  //     ScanComparison::kChanged);
}

}  // namespace maia
