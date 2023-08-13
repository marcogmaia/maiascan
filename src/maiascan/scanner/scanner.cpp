#include <afx.h>

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <Psapi.h>
#include <TlHelp32.h>
#include <fmt/core.h>

#include "maiascan/scanner/scanner.h"
#include "maiascan/scanner/types.h"

namespace maia {

namespace {

// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
// and compile with -DPSAPI_VERSION=1
Process GetProcNameAndId(DWORD pid) {
  TCHAR sz_process_name[1024] = TEXT("<unknown>");

  // Get a handle to the process.

  HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

  // Get the process name.
  if (nullptr != hproc) {
    HMODULE hmod;
    DWORD bytes_needed;

    if (EnumProcessModules(hproc, &hmod, sizeof(MemoryAddress), &bytes_needed)) {
      GetModuleBaseName(hproc, hmod, sz_process_name, sizeof(sz_process_name) / sizeof(TCHAR));
    }
  }

  CloseHandle(hproc);
  return {std::string(sz_process_name), pid};
}

}  // namespace

std::vector<Process> GetProcs() {
  // Get the list of process identifiers.
  DWORD bytes_needed{};
  std::array<DWORD, 1024> procs;
  if (!EnumProcesses(procs.data(), procs.size() * sizeof(Pid), &bytes_needed)) {
    return {};
  }

  std::span<DWORD> pids(procs.data(), bytes_needed / sizeof(DWORD));
  std::vector<Process> pids_found;
  pids_found.reserve(pids.size());
  for (auto &pid : pids) {
    if (pid) {
      pids_found.emplace_back(GetProcNameAndId(pid));
    }
  }

  return pids_found;
}

void ListProcs() {
  auto procs = GetProcs();
  for (auto &proc : procs) {
    std::cout << fmt::format("{} (PID: {})\n", proc.name, proc.pid);
  }
}

}  // namespace maia
