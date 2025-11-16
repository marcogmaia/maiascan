// Copyright (c) Maia

#include <Windows.h>

// #include <psapi.h>
// #include <tlhelp32.h>

#include <string>

#include "maia/application/process_selector_presenter.h"
#include "maia/logging.h"
#include "maia/mmem/mmem.h"

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

  auto desc = mmem::GetProcess(pid);
  return desc->name;
}

void RefreshProcesses(std::vector<ProcessInfo>& processes) {
  processes.clear();

  const auto get_processes = [&processes](const mmem::ProcessDescriptor& desc) {
    processes.emplace_back(desc.name, desc.pid);
    return true;
  };

  mmem::ListProcesses(get_processes);
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
  RefreshProcesses(process_list_);
}

}  // namespace maia
