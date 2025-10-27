// Copyright (c) Maia

#include <afx.h>

#include <array>
#include <span>
#include <string>
#include <vector>

#include <Psapi.h>
#include <TlHelp32.h>
#include <fmt/core.h>

#include "maia/scanner/memory_common.h"
#include "maia/scanner/scanner.h"

namespace maia {

namespace {

ProcessData GetProcNameAndId(DWORD pid) {
  TCHAR sz_process_name[1024] = TEXT("<unknown>");

  // Get a handle to the process.

  HANDLE hproc =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

  // Get the process name.
  if (hproc) {
    HMODULE hmod;
    DWORD bytes_needed;

    if (EnumProcessModules(
            hproc, &hmod, sizeof(MemoryAddress), &bytes_needed)) {
      GetModuleBaseName(hproc,
                        hmod,
                        sz_process_name,
                        sizeof(sz_process_name) / sizeof(TCHAR));
    }
  }

  CloseHandle(hproc);
  return {.name = std::string(sz_process_name), .pid = pid};
}

}  // namespace

std::vector<ProcessData> GetProcs() {
  // Get the list of process identifiers.
  DWORD bytes_needed{};
  std::array<DWORD, 1024> procs;
  if (!EnumProcesses(procs.data(), procs.size() * sizeof(Pid), &bytes_needed)) {
    return {};
  }

  std::span<DWORD> pids(procs.data(), bytes_needed / sizeof(DWORD));
  std::vector<ProcessData> pids_found;
  pids_found.reserve(pids.size());
  for (const auto& pid : pids) {
    if (pid) {
      pids_found.emplace_back(GetProcNameAndId(pid));
    }
  }

  return pids_found;
}

}  // namespace maia
