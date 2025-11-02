// Copyright (c) Maia

#include "maia/gui/widgets/process_selector.h"

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <string>

#include <imgui.h>

namespace maia::gui {

namespace {

std::string TCharToString(const TCHAR* tchar_str) {
#ifdef UNICODE
  std::wstring wstr(tchar_str);
  if (wstr.empty()) {
    return "";
  }
  int size_needed = WideCharToMultiByte(CP_UTF8,
                                        0,
                                        wstr.data(),
                                        static_cast<int>(wstr.size()),
                                        NULL,
                                        0,
                                        NULL,
                                        NULL);
  std::string str_to(size_needed, 0);
  WideCharToMultiByte(CP_UTF8,
                      0,
                      wstr.data(),
                      static_cast<int>(wstr.size()),
                      str_to.data(),
                      size_needed,
                      NULL,
                      NULL);
  return str_to;
#else
  return std::string(tchar_str);
#endif
}

std::string ToLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
  return str;
}

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

  if (GetModuleFileNameEx(h_process, nullptr, process_name, MAX_PATH)) {
    std::string full_path = TCharToString(process_name);
    size_t last_slash = full_path.find_last_of("\\/");
    CloseHandle(h_process);
    if (last_slash != std::string::npos) {
      return full_path.substr(last_slash + 1);
    } else {
      return full_path;
    }
  }

  CloseHandle(h_process);
  return TCharToString(process_name);
}

}  // namespace

void ProcessSelector::RenderProcessPickerButton() const {
  ImGui::Button("Pick (Drag Me)");

  if (ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::SetTooltip("Release over the target window to select.");
  }

  if (!ImGui::IsItemDeactivated()) {
    return;
  }

  // When the item is deactivated, then we pick the process under the cursor.
  POINT p;
  GetCursorPos(&p);
  HWND hwnd_under_cursor = WindowFromPoint(p);

  if (!hwnd_under_cursor) {
    return;
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd_under_cursor, &pid);
  if (pid != 0) {
    auto name = GetProcessNameFromPid(pid);
    signals_.process_selected.publish(pid, name);
  }
}

void ProcessSelector::Render(bool* p_open,
                             const std::vector<ProcessInfo>& processes,
                             const std::string& attached_process_name,
                             Pid attached_pid) {
  if (!p_open || !*p_open) {
    return;
  }

  ImGui::Begin("Process Selector", p_open);

  if (ImGui::Button("Refresh List")) {
    signals_.refresh_requested.publish();
  }

  ImGui::SameLine();
  ImGui::Text("%zu processes found.", processes.size());

  ImGui::SameLine();

  RenderProcessPickerButton();

  ImGui::InputText("Filter", filter_.data(), filter_.size());
  std::string filter_lower = ToLower(std::string(filter_.data()));

  ImGui::Separator();

  ImGui::Text("Selected Process: %s", attached_process_name.c_str());
  ImGui::Text("Selected PID: %u", attached_pid);

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

      bool is_selected = (proc.pid == attached_pid);
      if (ImGui::Selectable(item_label, is_selected)) {
        signals_.process_selected.publish(proc.pid, proc.name);
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
