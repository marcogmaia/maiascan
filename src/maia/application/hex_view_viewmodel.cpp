// Copyright (c) Maia

#include "maia/application/hex_view_viewmodel.h"

#include <imgui.h>

namespace maia {

HexViewViewModel::HexViewViewModel(ProcessModel& process_model,
                                   gui::HexViewModel& hex_model)
    : process_model_(process_model),
      hex_model_(hex_model) {
  process_model_.sinks()
      .ActiveProcessChanged()
      .connect<&gui::HexViewModel::SetProcess>(hex_model_);
}

void HexViewViewModel::GoToAddress(uintptr_t address) {
  is_visible_ = true;
  hex_model_.GoTo(address);
}

}  // namespace maia
