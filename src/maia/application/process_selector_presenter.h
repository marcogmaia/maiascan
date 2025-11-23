// Copyright (c) Maia

#pragma once

#include "maia/application/process_model.h"
#include "maia/gui/widgets/process_selector_view.h"

namespace maia {

class ProcessSelectorPresenter {
 public:
  ProcessSelectorPresenter(ProcessModel& process_model,
                           ProcessSelectorView& process_selector_view);

  void Render();

 private:
  void OnProcessPickRequested();
  void RefreshProcessList();
  void AttachProcess(Pid pid);

  ProcessModel& process_model_;
  ProcessSelectorView& process_selector_view_;

  std::vector<ProcessInfo> process_list_;
  std::string selected_process_name_ = "N/A";
  Pid selected_pid_ = 0;
};

}  // namespace maia
