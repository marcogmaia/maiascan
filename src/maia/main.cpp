// Copyright (c) Maia

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "gui/imgui_extensions.h"
#include "gui/widgets/process_picker.h"
#include "maia/logging.h"

namespace {

void ClearBackground(GLFWwindow* window, ImVec4& clear_color) {
  int display_w;
  int display_h;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(clear_color.x * clear_color.w,
               clear_color.y * clear_color.w,
               clear_color.z * clear_color.w,
               clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace

int main(int argc, const char** argv) {
  auto* window = static_cast<GLFWwindow*>(maia::ImGuiInit());
  if (!window) {
    maia::LogError("Failed to initialize the windowing subsystem.");
    return EXIT_FAILURE;
  }

  ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    maia::ImGuiBeginFrame();

    maia::gui::ShowProcessTool();
    ClearBackground(window, clear_color);

    maia::ImGuiEndFrame();
    glfwSwapBuffers(window);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  maia::ImGuiTerminate(window);

  return 0;
}
