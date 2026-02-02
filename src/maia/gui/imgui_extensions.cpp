// Copyright (c) Maia

#include "maia/gui/imgui_extensions.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <expected>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "maia/assets/resource.h"
#include "maia/logging.h"

namespace maia {

namespace {

void glfw_error_callback(int error, const char* description) {
  LogError("GLFW Error {}: {}", error, description);
}

std::expected<GLFWwindow*, int> InitGlfw() {
#ifdef _WIN32
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    return std::unexpected(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "maiascan", nullptr, nullptr);
  if (window == nullptr) {
    return std::unexpected(1);
  }
  glfwMakeContextCurrent(window);
  gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
  glfwSwapInterval(1);  // Enable vsync
  return window;
}

void TerminateGlfw(GLFWwindow* window) {
  if (window) {
    glfwDestroyWindow(window);
  }
  glfwTerminate();
}

void ImGuiProcessViewports() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
}

}  // namespace

GuiSystem::GuiSystem() {
  auto res = InitGlfw();
  if (!res) {
    return;
  }
  window_handle_ = *res;
  GLFWwindow* window = static_cast<GLFWwindow*>(window_handle_);

  SetWindowIcon(IDI_APP_ICON);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad
  // Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport
                                                       // / Platform Windows
  // io.ConfigViewportsNoAutoMerge = true;
  // io.ConfigViewportsNoTaskBarIcon = true;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform
  // windows can look identical to regular ones.
  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Decide GL+GLSL versions
#ifdef IMGUI_IMPL_OPENGL_ES2
  // GL ES 2.0 + GLSL 100
  const char* glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elifdef __APPLE__
  // GL 3.2 + GLSL 150
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required on Mac
#else
  // GL 4.3 + GLSL 430
  const char* glsl_version = "#version 430";
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+
  // only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only
#endif

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
}

GuiSystem::~GuiSystem() {
  if (IsValid()) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    TerminateGlfw(static_cast<GLFWwindow*>(window_handle_));
  }
}

void GuiSystem::BeginFrame() {
  if (IsValid()) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }
}

void GuiSystem::EndFrame() {
  if (IsValid()) {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGuiProcessViewports();
  }
}

bool GuiSystem::WindowShouldClose() const {
  if (!IsValid()) {
    return true;
  }
  return glfwWindowShouldClose(static_cast<GLFWwindow*>(window_handle_));
}

void GuiSystem::PollEvents() {
  glfwPollEvents();
}

void GuiSystem::SwapBuffers() {
  if (IsValid()) {
    glfwSwapBuffers(static_cast<GLFWwindow*>(window_handle_));
  }
}

void GuiSystem::SetWindowIcon(int resource_id) {
#ifdef _WIN32
  if (!IsValid()) {
    return;
  }
  HWND hwnd = glfwGetWin32Window(static_cast<GLFWwindow*>(window_handle_));
  HINSTANCE hInst = GetModuleHandle(nullptr);

  // Load the icon for both big and small sizes
  HICON hIconBig = (HICON)LoadImage(
      hInst, MAKEINTRESOURCE(resource_id), IMAGE_ICON, 32, 32, LR_SHARED);
  HICON hIconSmall = (HICON)LoadImage(
      hInst, MAKEINTRESOURCE(resource_id), IMAGE_ICON, 16, 16, LR_SHARED);

  if (hIconBig) {
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
  }
  if (hIconSmall) {
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
  }

  if (!hIconBig && !hIconSmall) {
    LogWarning("Failed to load icon from resource ID {}", resource_id);
  }
#endif
}

void GuiSystem::ClearWindow(float r, float g, float b, float a) {
  if (!IsValid()) {
    return;
  }
  GLFWwindow* window = static_cast<GLFWwindow*>(window_handle_);
  int display_w;
  int display_h;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace maia
