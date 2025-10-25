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
  std::wstring wStr(tcharStr);
  int size_needed = WideCharToMultiByte(
      CP_UTF8, 0, &wStr[0], (int)wStr.size(), NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8,
                      0,
                      &wStr[0],
                      (int)wStr.size(),
                      &strTo[0],
                      size_needed,
                      NULL,
                      NULL);
  return strTo;
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

// class ProcessPicker {};

// This function holds all our ImGui UI code
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

  // 2. Filter Input
  ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));
  std::string filter_lower = ToLower(std::string(filter));

  ImGui::Separator();

  // 3. Display Selected Process
  ImGui::Text("Selected Process: %s", selected_name.c_str());
  ImGui::Text("Selected PID: %lu",
              selected_pid);  // %lu is for DWORD (unsigned long)

  ImGui::Separator();

  // 4. Scrollable Process List
  ImGui::BeginChild("ProcessListRegion", ImVec2(0, 0), true);

  for (const auto& proc : processes) {
    std::string name_lower = ToLower(proc.name);

    // Apply the filter
    if (filter_lower.empty() || name_lower.contains(filter_lower)) {
      // Format the display string: "process.exe (PID: 1234)"
      char item_label[512];
      std::ignore = snprintf(item_label,
                             sizeof(item_label),
                             "%s (PID: %lu)",
                             proc.name.c_str(),
                             proc.pid);

      bool is_selected = (proc.pid == selected_pid);
      if (ImGui::Selectable(item_label, is_selected)) {
        // When clicked, update the selected PID and Name
        selected_pid = proc.pid;
        selected_name = proc.name;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();  // Auto-scroll to selected item
      }
    }
  }

  ImGui::EndChild();
  ImGui::End();
}

}  // namespace maia::gui
