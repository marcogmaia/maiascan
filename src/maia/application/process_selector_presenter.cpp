// Copyright (c) Maia

#include <string>

#include "maia/application/process_selector_presenter.h"
#include "maia/mmem/mmem.h"
#include "maia/mmem/process_utils.h"

namespace maia {

namespace {

void RefreshProcesses(std::vector<ProcessInfo>& processes) {
  processes.clear();

  const auto get_processes = [&processes](const mmem::ProcessDescriptor& desc) {
    processes.emplace_back(desc.name, desc.pid);
    return true;
  };

  mmem::ListProcesses(get_processes);
}

}  // namespace

void ProcessSelectorPresenter::OnProcessPickRequested() {
  if (auto pid = mmem::utils::GetProcessIdFromCursor()) {
    AttachProcess(*pid);
  }
}

void ProcessSelectorPresenter::AttachProcess(Pid pid) {
  if (process_model_.AttachToProcess(pid)) {
    selected_process_name_ = mmem::GetProcess(pid)->name;
    selected_pid_ = pid;
  } else {
    selected_process_name_ = "N/A";
    selected_pid_ = 0;
  }
}

ProcessSelectorPresenter::ProcessSelectorPresenter(
    ProcessModel& process_model, ProcessSelectorView& process_selector_view)
    : process_model_(process_model),
      process_selector_view_(process_selector_view) {
  // clang-format off
  process_selector_view.sinks().ProcessPickRequested().connect<&ProcessSelectorPresenter::OnProcessPickRequested>(*this);
  process_selector_view.sinks().RefreshRequested().connect<&ProcessSelectorPresenter::RefreshProcessList>(*this);
  process_selector_view.sinks().ProcessSelectedFromList().connect<&ProcessSelectorPresenter::AttachProcess>(*this);
  // clang-format on
  RefreshProcessList();
}

void ProcessSelectorPresenter::Render() {
  process_selector_view_.Render(
      nullptr, process_list_, selected_process_name_, selected_pid_);
}

void ProcessSelectorPresenter::RefreshProcessList() {
  RefreshProcesses(process_list_);
}

}  // namespace maia
