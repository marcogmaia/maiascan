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
Process GetProcNameAndId(DWORD processID) {
  TCHAR sz_process_name[1024] = TEXT("<unknown>");

  // Get a handle to the process.

  HANDLE h_process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

  // Get the process name.
  if (nullptr != h_process) {
    HMODULE h_mod;
    DWORD cb_needed;

    if (EnumProcessModules(h_process, &h_mod, sizeof(MemoryAddress), &cb_needed)) {
      GetModuleBaseName(h_process, h_mod, sz_process_name, sizeof(sz_process_name) / sizeof(TCHAR));
    }
  }

  CloseHandle(h_process);
  return {std::string(sz_process_name), processID};
}

}  // namespace

void TestScan() {}

// bool attach(Pid pid) {}

std::vector<Process> GetProcs() {
  // Get the list of process identifiers.
  DWORD bytes_needed{};
  std::array<DWORD, 1024> procs;
  if (!EnumProcesses(procs.data(), procs.size() * sizeof(Pid), &bytes_needed)) {
    return {};
  }

  std::span<DWORD> procs_span(procs.data(), bytes_needed / sizeof(DWORD));
  std::vector<Process> processed_found;
  processed_found.reserve(procs_span.size());
  for (auto &p : procs_span) {
    if (p) {
      processed_found.emplace_back(GetProcNameAndId(p));
    }
  }

  return processed_found;
}

void ListProcs() {
  auto procs = GetProcs();
  for (auto &proc : procs) {
    std::cout << fmt::format("{} (PID: {})\n", proc.name, proc.pid);
  }
}

}  // namespace maia
