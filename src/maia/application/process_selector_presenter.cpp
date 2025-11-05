// Copyright (c) Maia

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <string>

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

std::string GetLastErrorMessage(DWORD error_code) {
  if (error_code == 0) {
    return "No error";  // Or whatever you prefer for success
  }

  std::string message(512, 0);

  size_t size =
      FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr,
                     error_code,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     message.data(),
                     message.size(),
                     nullptr);

  if (size == 0) {
    // The error code couldn't be found or another error occurred.
    return "Unknown error " + std::to_string(error_code);
  }

  // Resize the string to the actual length of the message.
  message.resize(size);

  // Remove trailing newline characters that FormatMessage often adds.
  if (!message.empty()) {
    message.erase(message.find_last_not_of(" \n\r\t") + 1);
  }

  return message;
}

std::string GetProcessNameFromPid(DWORD pid) {
  if (pid == 0) {
    return "N/A";
  }

  HANDLE h_process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

  if (h_process == nullptr) {
    // TODO: this logging should be moved to where we first try to open the
    // process.
    auto errc = static_cast<uint32_t>(GetLastError());
    auto message = GetLastErrorMessage(errc);
    LogWarning("Error 0x{:04x}: {}", errc, message);
    return std::format("<{}>", message);
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
  // Must set the size.
  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(h_snapshot, &pe32)) {
    do {
      processes.emplace_back(TCharToString(pe32.szExeFile), pe32.th32ProcessID);
    } while (Process32Next(h_snapshot, &pe32));
  }

  CloseHandle(h_snapshot);
}

}  // namespace

void ProcessSelectorPresenter::OnProcessPickRequested() {
  POINT p;
  GetCursorPos(&p);
  HWND hwnd_under_cursor = WindowFromPoint(p);

  if (!hwnd_under_cursor) {
    return;
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd_under_cursor, &pid);
  AttachProcess(pid);
}

void ProcessSelectorPresenter::AttachProcess(Pid pid) {
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
        .Connect<&ProcessSelectorPresenter::RefreshProcessList>(process_selector_view_.signals().refresh_requested, *this)
        .Connect<&ProcessSelectorPresenter::AttachProcess>(process_selector_view.signals().process_selected_from_list, *this);
  // clang-format on
  RefreshProcessList();
}

void ProcessSelectorPresenter::Render() {
  process_selector_view_.Render(
      nullptr, process_list_, selected_process_name_, selected_pid_);
}

void ProcessSelectorPresenter::RefreshProcessList() {
  ::maia::RefreshProcessList(process_list_);
}

}  // namespace maia
