// Copyright (c) Maia

#include "maia/gui/layout.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace maia::gui {

void MakeDefaultLayout(ImGuiID dockspace_id) {
  // If the node already exists, we assume the layout is set (loaded from ini or
  // set previously).
  if (ImGui::DockBuilderGetNode(dockspace_id) != nullptr) {
    return;
  }

  // Clear any existing (partial) state and start fresh.
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id,
                                ImGui::GetMainViewport()->WorkSize);

  // --- Define Layout ---
  // We want:
  // [ Scanner (Left) | Results (Center) ]
  // [ Cheat Table (Bottom)              ]  <- Cheat Table (30% height)
  // Process Selector is in the menu bar, not docked.

  ImGuiID dock_main_id = dockspace_id;
  ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(
      dock_main_id, ImGuiDir_Down, 0.3f, nullptr, &dock_main_id);

  ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(
      dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);

  // --- Assign Windows to Regions ---
  ImGui::DockBuilderDockWindow("Scanner", dock_left_id);
  ImGui::DockBuilderDockWindow("Results", dock_main_id);
  ImGui::DockBuilderDockWindow("Cheat Table", dock_down_id);

  // Finalize
  ImGui::DockBuilderFinish(dockspace_id);
}

}  // namespace maia::gui
