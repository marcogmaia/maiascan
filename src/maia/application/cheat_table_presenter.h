// Copyright (c) Maia

#pragma once

#include "maia/application/cheat_table_model.h"
#include "maia/gui/widgets/cheat_table_view.h"

namespace maia {

class CheatTablePresenter {
 public:
  CheatTablePresenter(CheatTableModel& model, CheatTableView& view);

  void Render();

 private:
  void OnTableChanged();
  void OnFreezeToggled(size_t index);
  void OnDescriptionChanged(size_t index, std::string new_desc);
  void OnHexDisplayToggled(size_t index, bool show_as_hex);
  void OnTypeChangeRequested(size_t index, ScanValueType new_type);
  void OnValueChanged(size_t index, std::string new_val);
  void OnDeleteRequested(size_t index);
  void OnSaveRequested();
  void OnLoadRequested();
  void OnAddManualRequested(std::string address,
                            ScanValueType type,
                            std::string description);

  CheatTableModel& model_;
  CheatTableView& view_;
  std::vector<entt::scoped_connection> connections_;
  std::string last_save_path_;
};

}  // namespace maia
