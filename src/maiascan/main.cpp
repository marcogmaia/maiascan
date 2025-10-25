// Copyright (c) Maia

#include <iostream>
#include <print>

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <optional>

#include "gui/imgui_extensions.h"
#include "maiascan/console/commands.h"
#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scan.h"

namespace maia::scanner {

namespace {

template <typename T>
auto SearchT(Process& proc, T needle) {
  return Search(proc, ToBytesView(needle));
}

std::vector<MemoryAddress> GetAddressMatches(const Matches& matches) {
  size_t total_offsets = 0;
  for (const auto& match : matches) {
    total_offsets += match.offsets.size();
  }

  std::vector<MemoryAddress> addresses;
  addresses.reserve(total_offsets);
  for (const auto& match : matches) {
    for (const auto& offset : match.offsets) {
      addresses.emplace_back(NextAddress(match.page.address, offset));
    }
  }
  return addresses;
}

template <typename T>
std::vector<T> ReadAllValues(const Process& proc,
                             const std::vector<MemoryAddress>& addresses) {
  std::vector<T> values;
  values.reserve(addresses.size());

  for (const auto& addr : addresses) {
    T buffer{};
    auto res = proc.ReadIntoBuffer(addr, ToBytesView(buffer));
    if (!res) {
      std::cout << res.error() << std::endl;
    } else {
      values.emplace_back(buffer);
    }
  }

  return values;
}

void ProcessCommandAttach(console::CommandAttach& command) {
  auto pid = GetPidFromProcessName(command.process_name);
  if (!pid) {
    spdlog::error("Couldn't find the process: {}", command.process_name);
    return;
  }
  spdlog::info(
      "Selected process {} with (PID: {}).", command.process_name, *pid);
  auto proc = std::make_shared<Process>(*pid);
  int needle = 1337;
  Scan scan{proc};
  scan.Find(needle);
  for (auto& scan_entry : scan.scan()) {
    spdlog::info("{:>16} -- {}",
                 scan_entry.address,
                 BytesToFundamentalType<int>(scan_entry.bytes));
    int write_value = 2000;
    auto res = proc->Write(scan_entry.address, ToBytesView(write_value));
    if (!res) {
      spdlog::warn(res.error());
    }
  }
}

}  // namespace

}  // namespace maia::scanner

void glfw_error_callback(int error, const char* description) {
  std::println(stderr, "GLFW Error {}: {}", error, description);
}

std::expected<GLFWwindow*, int> InitGlfw() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    return std::unexpected(1);
  }

  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  GLFWwindow* window =
      glfwCreateWindow(1280, 720, "maiascan", nullptr, nullptr);
  if (window == nullptr) {
    return std::unexpected(1);
  }
  glfwMakeContextCurrent(window);
  gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
  glfwSwapInterval(1);  // Enable vsync
  return window;
}

void TerminateGlfw(GLFWwindow* window) {
  glfwDestroyWindow(window);
  glfwTerminate();
}

// TODO(marco): Refactor this garbage.
int main(int argc, const char** argv) {
  // ============ Setting up window
  auto glfw_init_result = InitGlfw();
  if (!glfw_init_result) {
    return glfw_init_result.error();
  }
  auto* window = *glfw_init_result;

  maia::ImGuiInit();

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  std::shared_ptr<maia::scanner::Process> proc;

  auto pid = maia::scanner::GetPidFromProcessName("fakegame");
  if (!pid) {
    spdlog::error("Make sure that fakegame is running");
    return 1;
  }
  proc = std::make_shared<maia::scanner::Process>(*pid);
  auto scan = maia::scanner::Scan{proc};

  std::string proc_name(256, 0);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    maia::ImGuiBeginFrame();

    if (proc) {
      if (ImGui::Begin("scan")) {
        static int needle{};
        ImGui::InputScalar("needle", ImGuiDataType_S32, &needle);
        if (ImGui::Button("Scan")) {
          scan.Find(needle);
          spdlog::info("Scanning for needle: {}", needle);
        }
        if (ImGui::Button("Remove different")) {
          scan.RemoveDifferent(needle);
        }
        if (!scan.scan().empty()) {
          auto total_matches = scan.scan().size();
          ImGui::Text("Number of matches: %zu", total_matches);
          if (total_matches < 2000) {
            for (auto& scan_entry : scan.scan()) {
              if (auto res = proc->ReadIntoBuffer(scan_entry.address,
                                                  scan_entry.bytes)) {
                ImGui::Text("%p -- %d",
                            scan_entry.address,
                            *reinterpret_cast<int*>(scan_entry.bytes.data()));
              } else {
                spdlog::warn(res.error());
              }
            }
          }
        }
      }
    }
    ImGui::End();

    ImGui::ShowDemoWindow();

    int display_w;
    int display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w,
                 clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w,
                 clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    maia::ImGuiEndFrame();
    glfwSwapBuffers(window);
  }

  maia::ImGuiTerminate();
  TerminateGlfw(window);

  return 0;
}
