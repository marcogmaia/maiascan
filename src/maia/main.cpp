// Copyright (c) Maia

#include <Windows.h>

#include <TlHelp32.h>

#include <numeric>

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>

#include "maia/gui/imgui_extensions.h"
#include "maia/gui/widgets/mapped_regions.h"
#include "maia/gui/widgets/process_picker.h"
#include "maia/gui/widgets/scan_widget.h"
#include "maia/logging.h"
#include "maia/scanner/livre_process_accessor.h"

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

class ScannerContext {
 public:
  explicit ScannerContext(entt::dispatcher& dispatcher) {
    dispatcher.sink<maia::gui::EventPickedProcess>()
        .connect<&ScannerContext::AttachToProcess>(this);
  }

  scanner::IProcessMemoryAccessor* GetProcessAccessor() {
    if (!process_accessor_) {
      return nullptr;
    }
    return &*process_accessor_;
  }

 private:
  void AttachToProcess(maia::gui::EventPickedProcess picked_process) {
    auto* handle = scanner::OpenHandle(picked_process.pid);
    if (!handle) {
      maia::LogWarning("Unable to attach to process: {}, PID: {}",
                       picked_process.name,
                       picked_process.pid);
      Reset();
      return;
    }
    process_accessor_.emplace(handle);
    process_name_ = picked_process.name;
    process_pid_ = picked_process.pid;
  }

  void Reset() {
    process_accessor_.reset();
    process_name_.reset();
    process_pid_.reset();
  }

  entt::sink<entt::sigh<void()>> sink_;

  std::optional<maia::scanner::LiveProcessAccessor> process_accessor_;
  std::optional<std::string> process_name_;
  std::optional<Pid> process_pid_;
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

  auto attacher = maia::ScannerContext(dispatcher);

  // clang-format off
  dispatcher.sink<maia::gui::EventPickedProcess>().connect<maia::ProcessPickedProcess>();
  dispatcher.sink<maia::gui::EventPickedProcess>().connect<maia::PrintPickedProcessInfo>();
  // clang-format on

  maia::gui::MappedRegionsWidget mapped_regions_widget;

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

    mapped_regions_widget.Render();

    maia::ClearBackground(window, clear_color);

    maia::ImGuiEndFrame();
    glfwSwapBuffers(window);

    dispatcher.update();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  maia::ImGuiTerminate(window);

  return 0;
}
