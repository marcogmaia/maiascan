#include <bit>
#include <iostream>
#include <type_traits>
#include <variant>

#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "app/imgui_extensions.hpp"
#include "maiascan/console/console.h"
#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scan.h"
#include "maiascan/scanner/scanner.h"

namespace maia::scanner {

namespace {

template <typename T>
auto SearchT(Process &proc, T needle) {
  return Search(proc, ToBytesView(needle));
}

std::vector<MemoryAddress> GetAddressMatches(const Matches &matches) {
  int total_offsets = 0;
  for (const auto &match : matches) {
    total_offsets += match.offsets.size();
  }

  std::vector<MemoryAddress> addresses;
  addresses.reserve(total_offsets);
  for (const auto &match : matches) {
    for (const auto &offset : match.offsets) {
      addresses.emplace_back(NextAddress(match.page.address, offset));
    }
  }
  return addresses;
}

void FilterOutChangedAddresses() {}

template <typename T>
std::vector<T> ReadAllValues(const Process &proc, const std::vector<MemoryAddress> &addresses) {
  std::vector<T> values;
  values.reserve(addresses.size());

  for (const auto &addr : addresses) {
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

template <CFundamentalType T>
T BytesToFundametalType(BytesView view) {
  auto *ptr = std::bit_cast<T *>(view.data());
  return *ptr;
}

void ProcessCommandAttach(console::CommandAttach &command) {
  auto pid = GetPidFromProcessName(command.process_name);
  if (!pid) {
    spdlog::error("Couldn't find the process: {}", command.process_name);
    return;
  }
  spdlog::info("Selected process {} with (PID: {}).", command.process_name, *pid);
  Process proc{static_cast<maia::Pid>(*pid)};
  int needle = 1337;
  Scan scan{&proc};
  scan.Find(needle);
  for (auto &scan_entry : scan.scan()) {
    spdlog::info("{:>16} -- {}", scan_entry.address, BytesToFundametalType<int>(scan_entry.bytes));
    proc.Write(scan_entry.address, ToBytesView(2000));
  }
}

}  // namespace

}  // namespace maia::scanner

void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

tl::expected<GLFWwindow *, int> InitGlfw() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    return tl::unexpected(1);
  }

  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  GLFWwindow *window = glfwCreateWindow(1280, 720, "maiascan", nullptr, nullptr);
  if (window == nullptr) {
    return tl::unexpected(1);
  }
  glfwMakeContextCurrent(window);
  gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
  glfwSwapInterval(1);  // Enable vsync
  return window;
}

void TerminateGlfw(GLFWwindow *window) {
  glfwDestroyWindow(window);
  glfwTerminate();
}

int main(int argc, const char **argv) {
  auto res = maia::console::Parse(argv, argc, true);
  if (!res) {
    spdlog::error("{}", res.error());
    return 1;
  }

  try {
    std::visit(maia::scanner::ProcessCommandAttach, *res);
  } catch (std::exception &) {
  }

  // ============ Setting up window
  auto glfw_init_result = InitGlfw();
  if (!glfw_init_result) {
    return glfw_init_result.error();
  }
  auto *window = *glfw_init_result;

  maia::ImGuiInit();

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    maia::ImGuiBeginFrame();

    if (ImGui::Begin("winname")) {
    }
    ImGui::End();

    ImGui::ShowDemoWindow();

    int display_w;
    int display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(
        clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    maia::ImGuiEndFrame();
    glfwSwapBuffers(window);
  }

  maia::ImGuiTerminate();
  TerminateGlfw(window);

  return 0;
}
