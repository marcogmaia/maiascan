// Copyright (c) Maia

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "process_picker.h"

#include <algorithm>
#include <string>
#include <vector>

#include <imgui.h>
#include <entt/signal/dispatcher.hpp>

#include "maia/core/memory_common.h"
#include "maia/logging.h"

namespace maia::gui {

namespace {

// Helper function to convert TCHAR (which can be wchar_t or char) to a
// std::string (UTF-8)
std::string TCharToString(const TCHAR* tchar_str) {
#ifdef UNICODE
  // If UNICODE is defined, TCHAR is wchar_t
  std::wstring wstr(tcharStr);
  int size_needed = WideCharToMultiByte(
      CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
  std::string strto(size_needed, 0);
  WideCharToMultiByte(CP_UTF8,
                      0,
                      &wstr[0],
                      (int)wstr.size(),
                      &strto[0],
                      size_needed,
                      NULL,
                      NULL);
  return strto;
#else
  // If UNICODE is not defined, TCHAR is char
  return std::string(tchar_str);
#endif
}

std::string ToLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
  return str;
}

// Function to refresh our list of running processes
void RefreshProcessList(std::vector<ProcessInfo>& processes) {
  processes.clear();

  HANDLE h_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (h_snapshot == INVALID_HANDLE_VALUE) {
    LogError("CreateToolhelp32Snapshot failed!");
    return;
  }

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);  // Must set the size

  // Get the first process
  if (Process32First(h_snapshot, &pe32)) {
    do {
      // Add the process info to our vector
      processes.push_back(
          {.name = TCharToString(pe32.szExeFile), .pid = pe32.th32ProcessID});
    } while (Process32Next(h_snapshot, &pe32));  // Get the next process
  }

  CloseHandle(h_snapshot);  // Clean up the snapshot object
}

// Helper function to get a process name from its PID
std::string GetProcessNameFromPid(DWORD pid) {
  if (pid == 0) {
    return "N/A";
  }

  HANDLE h_process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

  if (h_process == nullptr) {
    return "<Access Denied>";
  }

  TCHAR process_name[MAX_PATH] = TEXT("<unknown>");

  // Get the full path of the module
  if (GetModuleFileNameEx(h_process, nullptr, process_name, MAX_PATH)) {
    // Convert TCHAR to std::string
    std::string full_path = TCharToString(process_name);

    // Find the last backslash to get just the filename
    size_t last_slash = full_path.find_last_of("\\/");
    if (last_slash != std::string::npos) {
      CloseHandle(h_process);
      return full_path.substr(last_slash + 1);
    } else {
      CloseHandle(h_process);
      return full_path;  // Return full path if no slash found
    }
  }

  CloseHandle(h_process);
  return TCharToString(process_name);
}

// Renders a "Pick (Drag Me)" button.
// When the button is *released*, this function finds the process
// (PID and name) that owns the window directly under the cursor.
// It returns the "ProcessInfo" on that specific frame,
// and std::nullopt on all other frames.
std::optional<ProcessInfo> ButtonProcessPicker() {
  ImGui::Button("Pick (Drag Me)");

  // Check if the user is clicking and holding the button
  if (ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::SetTooltip("Release over the target window to select.");
  }

  if (!ImGui::IsItemDeactivated()) {
    return std::nullopt;
  }

  POINT p;
  GetCursorPos(&p);
  HWND hwnd_under_cursor = WindowFromPoint(p);

  if (!hwnd_under_cursor) {
    return std::nullopt;
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd_under_cursor, &pid);
  if (pid == 0) {
    return std::nullopt;
  }

  auto name = GetProcessNameFromPid(pid);
  return ProcessInfo{.name = name, .pid = pid};
}

}  // namespace

void ShowProcessTool(entt::dispatcher& dispatcher, bool* p_open) {
  // Static variables persist between frames
  static std::vector<ProcessInfo> processes;
  static char filter[MAX_PATH] = "";
  static DWORD selected_pid = 0;
  static std::string selected_name = "None";

  // Refresh the list when the window is first opened.
  if (processes.empty()) {
    RefreshProcessList(processes);
  }

  ImGui::Begin("Process Selector", p_open);

  if (ImGui::Button("Refresh List")) {
    RefreshProcessList(processes);
  }
  ImGui::SameLine();
  ImGui::Text("%zu processes found.", processes.size());

  ImGui::SameLine();

  if (auto proc_picked = ButtonProcessPicker(); proc_picked) {
    RefreshProcessList(processes);
    selected_name = proc_picked->name;
    selected_pid = proc_picked->pid;
    dispatcher.enqueue(
        EventPickedProcess{.pid = selected_pid, .name = selected_name});
  }

  ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));
  std::string filter_lower = ToLower(std::string(filter));

  ImGui::Separator();

  ImGui::Text("Selected Process: %s", selected_name.c_str());
  ImGui::Text("Selected PID: %lu", selected_pid);

  ImGui::Separator();

  ImGui::BeginChild("ProcessListRegion", ImVec2(0, 0), true);

  for (const auto& proc : processes) {
    std::string name_lower = ToLower(proc.name);

    if (filter_lower.empty() || name_lower.contains(filter_lower)) {
      char item_label[512];
      std::ignore = snprintf(item_label,
                             sizeof(item_label),
                             "%s (PID: %u)",
                             proc.name.c_str(),
                             proc.pid);

      bool is_selected = (proc.pid == selected_pid);
      if (ImGui::Selectable(item_label, is_selected)) {
        selected_pid = proc.pid;
        selected_name = proc.name;
        dispatcher.enqueue(
            EventPickedProcess{.pid = selected_pid, .name = selected_name});
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
  }

  ImGui::EndChild();
  ImGui::End();
}

}  // namespace maia::gui
