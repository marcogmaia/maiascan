// Copyright (c) Maia

#pragma once

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <iostream>
#include <string>
#include <vector>

#include <imgui.h>

namespace maia::gui {

// A simple struct to hold our process information
struct ProcessInfo {
  DWORD pid;
  std::string name;
};

// Helper function to convert TCHAR (which can be wchar_t or char) to a
// std::string (UTF-8)
std::string TCharToString(const TCHAR* tcharStr) {
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
  return std::string(tcharStr);
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
    std::cerr << "CreateToolhelp32Snapshot failed!\n";
    return;
  }

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);  // Must set the size

  // Get the first process
  if (Process32First(h_snapshot, &pe32)) {
    do {
      // Add the process info to our vector
      processes.push_back(
          {.pid = pe32.th32ProcessID, .name = TCharToString(pe32.szExeFile)});
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

void ShowProcessTool(bool* p_open = nullptr) {
  // Static variables persist between frames
  static std::vector<ProcessInfo> processes;
  static char filter[256] = "";
  static DWORD selected_pid = 0;
  static std::string selected_name = "None";

  // Refresh the list when the window is first opened
  if (processes.empty()) {
    RefreshProcessList(processes);
  }

  ImGui::Begin("Process Selector", p_open);

  // 1. Refresh Button
  if (ImGui::Button("Refresh List")) {
    RefreshProcessList(processes);
  }
  ImGui::SameLine();
  ImGui::Text("%zu processes found.", processes.size());

  // --- "Drag-and-Drop" Picker Button ---
  ImGui::SameLine();
  ImGui::Button("Pick (Drag Me)");

  // Check if the user is clicking and holding the button
  if (ImGui::IsItemActive()) {
    // --- NEW: Get HWND from ImGui ---
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    HWND hwnd = static_cast<HWND>(viewport->PlatformHandle);

    // 1. Set the system-wide cursor to a crosshair
    SetCapture(hwnd);
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);  // Hide ImGui's cursor
    SetCursor(LoadCursor(NULL, IDC_CROSS));
    ImGui::SetTooltip("Release over the target window to select.");
  }

  // Check if the user *released* the mouse after holding the button
  if (ImGui::IsItemDeactivated()) {
    // 1. Always release mouse capture!
    ReleaseCapture();

    // --- NEW: Get HWND from ImGui ---
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    HWND hwnd = static_cast<HWND>(viewport->PlatformHandle);

    // 2. Get mouse position
    POINT p;
    GetCursorPos(&p);

    // 3. Hide our own window *briefly*
    ShowWindow(hwnd, SW_HIDE);
    HWND h_wnd_under_cursor = WindowFromPoint(p);
    ShowWindow(hwnd, SW_SHOW);  // Show it again immediately

    // 4. Get PID and Name
    if (h_wnd_under_cursor && h_wnd_under_cursor != hwnd) {
      DWORD pid = 0;
      GetWindowThreadProcessId(h_wnd_under_cursor, &pid);
      if (pid != 0) {
        // Update our selected process
        selected_pid = pid;
        selected_name = GetProcessNameFromPid(pid);
      }
    }
  }
  // --- END of new picker logic ---

  // 2. Filter Input
  ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));
  std::string filter_lower = ToLower(std::string(filter));

  ImGui::Separator();

  // 3. Display Selected Process
  ImGui::Text("Selected Process: %s", selected_name.c_str());
  ImGui::Text("Selected PID: %lu", selected_pid);

  ImGui::Separator();

  // 4. Scrollable Process List
  ImGui::BeginChild("ProcessListRegion", ImVec2(0, 0), true);

  for (const auto& proc : processes) {
    std::string name_lower = ToLower(proc.name);

    if (filter_lower.empty() || name_lower.contains(filter_lower)) {
      char item_label[512];
      std::ignore = snprintf(item_label,
                             sizeof(item_label),
                             "%s (PID: %lu)",
                             proc.name.c_str(),
                             proc.pid);

      bool is_selected = (proc.pid == selected_pid);
      if (ImGui::Selectable(item_label, is_selected)) {
        selected_pid = proc.pid;
        selected_name = proc.name;
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
