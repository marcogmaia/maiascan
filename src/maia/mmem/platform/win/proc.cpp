// Copyright (c) Maia

#include <Windows.h>

#include "maia/mmem/proc.h"

#include <bit>
#include <span>

namespace maia::mmem {

HANDLE OpenProcess(DWORD pid, DWORD access) {
  if (pid == GetCurrentProcessId()) {
    return GetCurrentProcess();
  }

  return ::OpenProcess(access, FALSE, pid);
}

size_t ReadMemoryEx(const ProcessDescriptor* process,
                    uintptr_t source,
                    std::span<std::byte> dest) {
  HANDLE hproc;
  SIZE_T bytes_read;

  if (!process || source == UINTPTR_MAX || dest.empty()) {
    return 0;
  }

  hproc = OpenProcess(process->pid, PROCESS_VM_READ);
  if (!hproc) {
    return 0;
  }

  if (!::ReadProcessMemory(hproc,
                           std::bit_cast<void*>(source),
                           dest.data(),
                           dest.size_bytes(),
                           &bytes_read)) {
    bytes_read = 0;
  }

  ::CloseHandle(hproc);
  return bytes_read;
}

}  // namespace maia::mmem
