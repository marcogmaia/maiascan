// Copyright (c) Maia

#pragma once

#include <entt/entt.hpp>

#include "maia/application/process_model.h"
#include "maia/gui/models/ui_state.h"
#include "maia/gui/widgets/process_selector_view.h"

namespace maia {

class ProcessSelectorViewModel {
 public:
  ProcessSelectorViewModel(ProcessModel& process_model,
                           gui::ProcessSelectorState& state);

  void OnProcessPickRequested();
  void RefreshProcessList();
  void AttachProcess(Pid pid);
  void OnRefreshRequested();

 private:
  ProcessModel& process_model_;
  gui::ProcessSelectorState& state_;

  std::vector<entt::scoped_connection> connections_;
};

}  // namespace maia
