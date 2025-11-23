// Copyright (c) Maia

#include "maia/application/scanner_presenter.h"

namespace maia {

namespace {

/// \brief Internal tag type used for compile-time function pointer deduction.
template <auto Candidate>
struct SlotTag {};

/// \brief Compile-time constant wrapper for function pointers.
/// \details Usage: maia::Slot<&Class::Method> or maia::Slot<&FreeFunction>.
template <auto Candidate>
constexpr SlotTag<Candidate> Slot = {};  // NOLINT

/// \brief Connects a signal to a member function and manages connection
/// lifetime.
/// \param storage Container to hold the scoped connection (e.g., std::vector).
/// \param sink The source signal or sink.
/// \param instance The receiver object instance.
/// \param tag The slot wrapper (use maia::Slot<&Class::Method>).
template <typename Storage, typename Sink, typename Receiver, auto Candidate>
void Connect(Storage& storage,
             Sink&& sink,
             Receiver* instance,
             SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>(instance));
};

/// \brief Connects a signal to a free function or static method.
/// \param storage Container to hold the scoped connection.
/// \param sink The source signal or sink.
/// \param tag The slot wrapper (use maia::Slot<&Function>).
template <typename Storage, typename Sink, auto Candidate>
void Connect(Storage& storage, Sink&& sink, SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>());
}

}  // namespace

ScannerPresenter::ScannerPresenter(ScanResultModel& scan_result_model,
                                   ProcessModel& process_model,
                                   ScannerWidget& scanner_widget)
    : scan_result_model_(scan_result_model),
      process_model_(process_model),
      scanner_widget_(scanner_widget) {
  // clang-format off

  connections_.emplace_back(process_model_.sinks().ActiveProcessChanged().connect<&ScanResultModel::SetActiveProcess>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().NewScanPressed().connect<&ScanResultModel::FirstScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().NextScanPressed().connect<&ScanResultModel::NextScan>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().ScanComparisonSelected().connect<&ScanResultModel::SetScanComparison>(scan_result_model_));
  connections_.emplace_back(scanner_widget_.sinks().TargetValueSelected().connect<&ScanResultModel::SetTargetScanValue>(scan_result_model_));

  // connections_.emplace_back(scanner_widget_.sinks().AutoUpdateChanged().connect<&ScannerPresenter::OnAutoUpdateChanged>(*this));
  // I don't know if I'll keep this signature here, I'm just exploring ideas on how to mimic Qt's connection for signal/slow.
  Connect(connections_, scanner_widget_.sinks().AutoUpdateChanged(), this, Slot<&ScannerPresenter::OnAutoUpdateChanged>);

  // clang-format on

  // TODO: Set initial state.
  // scanner_widget_.signals().scan_comparison_selected.publish(
  //     ScanComparison::kChanged);
}

// void ScannerPresenter::Render() {
//     bool scanning = model_.IsScanning();
//     controls_view_.Render(scanning);
//     // The Presenter grabs the 'const &' from the model and passes it to
//     View.
//     // No copying happens here. It's just a pointer pass.
//     const TableStorage& data = model_.GetDisplayResults();
//     table_view_.Render(data);
// }

void ScannerPresenter::OnAutoUpdateChanged(bool is_checked) {
  if (is_checked) {
    scan_result_model_.StartAutoUpdate();
  } else {
    scan_result_model_.StopAutoUpdate();
  }
}

}  // namespace maia
