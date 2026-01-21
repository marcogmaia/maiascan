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
  void OnValueChanged(size_t index, std::string new_val);
  void OnDeleteRequested(size_t index);

  CheatTableModel& model_;
  CheatTableView& view_;
  std::vector<entt::scoped_connection> connections_;
};

}  // namespace maia
