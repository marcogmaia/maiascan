// Copyright (c) Maia

#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>

#include "maia/application/cheat_table_model.h"
#include "maia/application/process_model.h"
#include "maia/gui/models/ui_state.h"

namespace maia {

class CheatTableViewModel {
 public:
  CheatTableViewModel(CheatTableModel& model,
                      ProcessModel& process_model,
                      gui::CheatTableState& state);

  void OnFreezeToggled(size_t index);
  void OnDescriptionChanged(size_t index, std::string new_desc);
  void OnHexDisplayToggled(size_t index, bool show_as_hex);
  void OnValueChanged(size_t index, std::string new_val);
  void OnTypeChangeRequested(size_t index, ScanValueType new_type);
  void OnDeleteRequested(size_t index);
  void OnSaveRequested();
  void OnLoadRequested();
  void OnAddManualRequested(std::string address,
                            ScanValueType type,
                            std::string description);

 private:
  CheatTableModel& model_;
  ProcessModel& process_model_;
  gui::CheatTableState& state_;
  std::string last_save_path_;
};

}  // namespace maia
