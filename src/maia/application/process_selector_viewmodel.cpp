// Copyright (c) Maia

#include "maia/application/process_selector_viewmodel.h"

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

void ProcessSelectorViewModel::OnProcessPickRequested() {
  if (auto pid = mmem::utils::GetProcessIdFromCursor()) {
    AttachProcess(*pid);
  }
}

void ProcessSelectorViewModel::AttachProcess(Pid pid) {
  if (process_model_.AttachToProcess(pid)) {
    state_.attached_process_name = mmem::GetProcess(pid)->name;
    state_.attached_pid = pid;
  } else {
    state_.attached_process_name = "N/A";
    state_.attached_pid = 0;
  }
}

ProcessSelectorViewModel::ProcessSelectorViewModel(
    ProcessModel& process_model, gui::ProcessSelectorState& state)
    : process_model_(process_model),
      state_(state) {
  RefreshProcessList();
}

void ProcessSelectorViewModel::OnRefreshRequested() {
  RefreshProcessList();
}

void ProcessSelectorViewModel::RefreshProcessList() {
  RefreshProcesses(state_.processes);
}

}  // namespace maia
