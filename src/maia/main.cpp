// Copyright (c) Maia

#include <Windows.h>

#include <TlHelp32.h>

#include <memory>
#include <numeric>

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

#include "gui/imgui_extensions.h"
#include "gui/widgets/process_picker.h"
#include "gui/widgets/scan_widget.h"
#include "maia/logging.h"
#include "maia/scanner/process.h"

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

void ProcessPickedProcess(maia::gui::EventPickedProcess picked_process) {
  maia::LogInfo("PID: {}, Name: {}", picked_process.pid, picked_process.name);
}

class ProcessAttacher {
 public:
  explicit ProcessAttacher(entt::dispatcher& dispatcher) {
    dispatcher.sink<maia::gui::EventPickedProcess>()
        .connect<&ProcessAttacher::AttachToProcess>(this);
  }

  maia::scanner::IProcessMemoryAccessor* GetProcessAccessor() {
    if (!process_accessor_) {
      return nullptr;
    }
    return process_accessor_.get();
  }

 private:
  void AttachToProcess(maia::gui::EventPickedProcess picked_process) {
    // Open the process with the necessary permissions
    HANDLE handle =
        OpenProcess(PROCESS_QUERY_INFORMATION |  // Required for VirtualQueryEx
                        PROCESS_VM_READ |   // Required for ReadProcessMemory
                        PROCESS_VM_WRITE |  // Required for WriteProcessMemory
                        PROCESS_VM_OPERATION,  // Required for VirtualProtectEx
                    FALSE,
                    picked_process.pid);
    if (!handle) {
      maia::LogWarning("Unable to attach to process: {}, PID: {}",
                       picked_process.name,
                       picked_process.pid);
      return;
    }
    process_accessor_ =
        std::make_unique<maia::scanner::LiveProcessAccessor>(handle);
  }

  std::unique_ptr<maia::scanner::LiveProcessAccessor> process_accessor_;
};

size_t GetTotalOccupiedMemory(
    const std::vector<maia::MemoryRegion>& mem_regions) {
  return std::accumulate(mem_regions.begin(),
                         mem_regions.end(),
                         0z,
                         [](size_t total, const maia::MemoryRegion& region) {
                           return total + region.size;
                         });
}

MemoryAddress GetBaseAddress(Pid pid) {
  auto* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return {};
  }
  MODULEENTRY32 mod_entry{.dwSize = sizeof(MODULEENTRY32)};
  bool success = Module32First(snapshot, &mod_entry) != 0;
  if (!success) {
    auto err = GetLastError();
    return {};
  }
  return std::bit_cast<MemoryAddress>(mod_entry.modBaseAddr);
}

void PrintAllProcessModules(Pid pid) {
  HANDLE hsnapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

  if (hsnapshot == INVALID_HANDLE_VALUE) {
    return;
  }

  MODULEENTRY32 mod_entry = {.dwSize = 0};
  mod_entry.dwSize = sizeof(MODULEENTRY32);
  for (bool ok = Module32First(hsnapshot, &mod_entry) != 0; ok;
       ok = Module32Next(hsnapshot, &mod_entry) != 0) {
    fmt::print("{:20} -- Addr: {:p}\n",
               mod_entry.szModule,
               fmt::ptr(mod_entry.modBaseAddr));
  }

  CloseHandle(hsnapshot);
}

void PrintPickedProcessInfo(const gui::EventPickedProcess& picked_process) {
  PrintAllProcessModules(picked_process.pid);
  fmt::print("BaseAddress: {:p}\n",
             std::bit_cast<const void*>(GetBaseAddress(picked_process.pid)));
}

}  // namespace

}  // namespace maia

int main() {
  auto* window = static_cast<GLFWwindow*>(maia::ImGuiInit());
  if (!window) {
    maia::LogError("Failed to initialize the windowing subsystem.");
    return EXIT_FAILURE;
  }

  ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);

  entt::dispatcher dispatcher;

  auto attacher = maia::ProcessAttacher(dispatcher);

  // clang-format off
  dispatcher.sink<maia::gui::EventPickedProcess>().connect<maia::ProcessPickedProcess>();
  dispatcher.sink<maia::gui::EventPickedProcess>().connect<maia::PrintPickedProcessInfo>();
  // clang-format on

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    maia::ImGuiBeginFrame();

    maia::gui::ShowProcessTool(dispatcher);

    auto* proc_access = attacher.GetProcessAccessor();
    if (proc_access) {
      maia::gui::ShowMemoryScannerWindow();
      auto regions = proc_access->GetMemoryRegions();
      auto total_size_bytes = maia::GetTotalOccupiedMemory(regions);
      maia::LogInfo(
          "Num memory regions: {}, total size in bytes: {} ({:.3f}MB)",
          regions.size(),
          total_size_bytes,
          static_cast<double>(total_size_bytes) / (1 << 20));
      break;
    }

    maia::ClearBackground(window, clear_color);

    maia::ImGuiEndFrame();
    glfwSwapBuffers(window);

    dispatcher.update();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  maia::ImGuiTerminate(window);

  return 0;
}
