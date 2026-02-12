// Copyright (c) Maia

#include "maia/gui/widgets/process_selector_view.h"

#include <algorithm>
#include <string>

#include <imgui.h>

namespace maia {

namespace {

std::string ToLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
  return str;
}

}  // namespace

void ProcessSelectorView::RenderProcessPickerButton() const {
  ImGui::Button("Pick (Drag Me)");

  if (ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::SetTooltip("Release over the target window to select.");
  }

  if (ImGui::IsItemDeactivated()) {
    signals_.process_pick_requested.publish();
  }
}

void ProcessSelectorView::Render(bool* p_open,
                                 const std::vector<ProcessInfo>& processes,
                                 const std::string& attached_process_name,
                                 Pid attached_pid) {
  ImGui::Begin("Process Selector", p_open);

  if (ImGui::Button("Refresh List")) {
    signals_.refresh_requested.publish();
  }

  ImGui::SameLine();
  ImGui::Text("%zu processes found.", processes.size());

  ImGui::SameLine();

  RenderProcessPickerButton();

  if (ImGui::InputText("Filter", filter_.data(), filter_.size())) {
    signals_.refresh_requested.publish();
  }
  std::string filter_lower = ToLower(std::string(filter_.data()));

  ImGui::Separator();

  ImGui::Text("Selected Process: %s", attached_process_name.c_str());
  ImGui::Text("Selected PID: %u", attached_pid);

  ImGui::Separator();

  ImGui::BeginChild("ProcessListRegion", ImVec2(0, 0), true);

  for (const auto& proc : processes) {
    std::string name_lower = ToLower(proc.name);

    if (filter_lower.empty() || name_lower.contains(filter_lower)) {
      static char item_label[512];
      std::ignore = snprintf(item_label,
                             sizeof(item_label),
                             "%s (PID: %u)",
                             proc.name.c_str(),
                             proc.pid);

      bool is_selected = (proc.pid == attached_pid);
      if (ImGui::Selectable(item_label, is_selected)) {
        signals_.process_selected_from_list.publish(proc.pid);
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
  }

  ImGui::EndChild();
  ImGui::End();
}

void ProcessSelectorView::RenderToolbar(
    const std::string& attached_process_name,
    Pid attached_pid,
    bool* show_window) {
  if (attached_pid != 0) {
    ImGui::Text(
        "Process: %s (PID: %u)", attached_process_name.c_str(), attached_pid);
  } else {
    ImGui::TextDisabled("No Process");
  }
  ImGui::SameLine();
  if (ImGui::Button("Select...")) {
    *show_window = true;
  }
}

}  // namespace maia
