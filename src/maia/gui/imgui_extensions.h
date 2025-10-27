// Copyright (c) Maia

#pragma once

#include <imgui.h>

namespace maia {

// TODO: Make these a RAII construct. Also change to something to indicate that
// we are initializing the windowing system. Returns the window handle.
void* ImGuiInit();
void ImGuiTerminate(void* window_handle);

// Update and Render additional Platform Windows
// (Platform functions may change the current OpenGL context, so we save/restore
// it to make it easier to paste this code elsewhere.
//  For this specific demo app we could also call glfwMakeContextCurrent(window)
//  directly)
void ImGuiProcessViewports();

void ImGuiBeginFrame();

void ImGuiEndFrame();

}  // namespace maia
