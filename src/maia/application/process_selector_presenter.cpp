// Copyright (c) Maia

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "maia/application/process_selector_presenter.h"
#include "maia/logging.h"

namespace maia {

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

}  // namespace

void ProcessSelectorPresenter::OnProcessPickRequested() {
  // When the item is deactivated, then we pick the process under the cursor.
  POINT p;
  GetCursorPos(&p);
  HWND hwnd_under_cursor = WindowFromPoint(p);

  if (!hwnd_under_cursor) {
    return;
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd_under_cursor, &pid);
  if (process_model_.AttachToProcess(pid)) {
    selected_process_name_ = GetProcessNameFromPid(pid);
    selected_pid_ = pid;
  }
}

ProcessSelectorPresenter::ProcessSelectorPresenter(
    ProcessModel& process_model, ProcessSelectorView& process_selector_view)
    : process_model_(process_model),
      process_selector_view_(process_selector_view) {
  // clang-format off
  sinks_.Connect<&ProcessSelectorPresenter::OnProcessPickRequested>(process_selector_view_.signals().process_pick_requested, *this)
        .Connect<&ProcessSelectorPresenter::OnRefreshProcessList>(process_selector_view_.signals().refresh_requested, *this);
  // clang-format on
}

void ProcessSelectorPresenter::Render() {
  process_selector_view_.Render(
      nullptr, process_list_, selected_process_name_, selected_pid_);
}

}  // namespace maia
