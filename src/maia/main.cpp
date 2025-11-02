// Copyright (c) Maia

// #include <numeric>

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glad/glad.h>
#include <entt/signal/dispatcher.hpp>

#include "maia/application/process_selector_presenter.h"
#include "maia/gui/imgui_extensions.h"
#include "maia/logging.h"
#include "scanner/process_attacher.h"

namespace maia {

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

// size_t GetTotalOccupiedMemory(
//     const std::vector<maia::MemoryRegion>& mem_regions) {
//   return std::accumulate(mem_regions.begin(),
//                          mem_regions.end(),
//                          0z,
//                          [](size_t total, const maia::MemoryRegion& region) {
//                            return total + region.size;
//                          });
// }

// MemoryAddress GetBaseAddress(Pid pid) {
//   auto* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
//   if (snapshot == INVALID_HANDLE_VALUE) {
//     return {};
//   }
//   MODULEENTRY32 mod_entry{.dwSize = sizeof(MODULEENTRY32)};
//   bool success = Module32First(snapshot, &mod_entry) != 0;
//   if (!success) {
//     auto err = GetLastError();
//     return {};
//   }
//   return std::bit_cast<MemoryAddress>(mod_entry.modBaseAddr);
// }

// void PrintAllProcessModules(Pid pid) {
//   HANDLE hsnapshot =
//       CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

//   if (hsnapshot == INVALID_HANDLE_VALUE) {
//     return;
//   }

//   MODULEENTRY32 mod_entry = {.dwSize = 0};
//   mod_entry.dwSize = sizeof(MODULEENTRY32);
//   for (bool ok = Module32First(hsnapshot, &mod_entry) != 0; ok;
//        ok = Module32Next(hsnapshot, &mod_entry) != 0) {
//     fmt::print("{:20} -- Addr: {:p}\n",
//                mod_entry.szModule,
//                fmt::ptr(mod_entry.modBaseAddr));
//   }

//   CloseHandle(hsnapshot);
// }

}  // namespace

}  // namespace maia

int main() {
  maia::LogInstallFormat();

  auto* window = static_cast<GLFWwindow*>(maia::ImGuiInit());
  if (!window) {
    maia::LogError("Failed to initialize the windowing subsystem.");
    return EXIT_FAILURE;
  }

  ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);

  maia::ProcessAttacher process_attacher{};
  maia::ProcessModel process_model{process_attacher};
  maia::ProcessSelectorView proc_selector_view{};
  maia::ProcessSelectorPresenter process_selector{process_model,
                                                  proc_selector_view};

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    maia::ImGuiBeginFrame();
    process_selector.Render();
    maia::ClearBackground(window, clear_color);

    maia::ImGuiEndFrame();
    glfwSwapBuffers(window);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  maia::ImGuiTerminate(window);

  return 0;
}
