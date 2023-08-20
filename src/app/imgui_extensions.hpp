#pragma once

#include <tuple>

#include <imgui.h>

namespace maia {

void ImGuiInit();
void ImGuiTerminate();

// Update and Render additional Platform Windows
// (Platform functions may change the current OpenGL context, so we save/restore
// it to make it easier to paste this code elsewhere.
//  For this specific demo app we could also call glfwMakeContextCurrent(window)
//  directly)
void ImGuiProcessViewports();

void ImGuiBeginFrame();

void ImGuiEndFrame();

}  // namespace maia
