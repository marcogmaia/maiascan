#include "maia/application/hex_view_viewmodel.h"
#include <imgui.h>

namespace maia {

HexViewViewModel::HexViewViewModel(ProcessModel& process_model,
                                   gui::HexViewModel& hex_model,
                                   gui::HexView& hex_view)
    : process_model_(process_model),
      hex_model_(hex_model),
      hex_view_(hex_view) {
  process_model_.sinks()
      .ActiveProcessChanged()
      .connect<&gui::HexViewModel::SetProcess>(hex_model_);
}

void HexViewViewModel::Render() {
  if (!is_visible_) {
    return;
  }

  if (ImGui::Begin("Memory Viewer", &is_visible_)) {
    hex_view_.Render();
  }
  ImGui::End();
}

void HexViewViewModel::GoToAddress(uintptr_t address) {
  is_visible_ = true;
  hex_model_.GoTo(address);
}

}  // namespace maia
