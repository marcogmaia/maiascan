// Copyright (c) Maia

#include <Windows.h>

#include "maia/mmem/mmem.h"

#include <Psapi.h>
#include <TlHelp32.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace maia::mmem {

namespace {

// RAII helper for process handles.
using ProcessHandle =
    // Use void* as the "type" for the handle.
    std::unique_ptr<void, decltype([](void* h) {
                      // Don't close the pseudo-handle for the current process
                      if (h && h != ::GetCurrentProcess()) {
                        ::CloseHandle(static_cast<HANDLE>(h));
                      }
                    })>;

// Platform helper
ProcessHandle OpenProcessHandle(uint32_t pid, DWORD access) {
  HANDLE h = nullptr;
  if (pid == GetCurrentProcessId()) {
    h = ::GetCurrentProcess();
  } else {
    h = ::OpenProcess(access, FALSE, pid);
  }
  // Let the unique_ptr manage the handle
  return ProcessHandle(h);
}

// Helper to extract filename from path
std::string_view GetFileName(std::string_view path) {
  auto pos = path.find_last_of('\\');
  if (pos == std::string_view::npos) {
    pos = path.find_last_of('/');
  }
  return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
}

// Convert Windows protection to our Protection enum
Protection WinProtectionToEnum(DWORD protect) {
  // Mask out modifier bits (GUARD, NOCACHE, WRITECOMBINE)
  // clang-format off
  switch (protect & 0xFF) {
    case PAGE_EXECUTE_READWRITE: return Protection::kExecuteReadWrite;
    case PAGE_EXECUTE_READ:      [[fallthrough]];
    case PAGE_EXECUTE_WRITECOPY: return Protection::kExecuteRead;  // Approximate
    case PAGE_EXECUTE:           return Protection::kExecute;
    case PAGE_READWRITE:         return Protection::kReadWrite;
    case PAGE_WRITECOPY:         [[fallthrough]];
    case PAGE_READONLY:          return Protection::kRead;
    default:
      return Protection::kNone;
  }
  // clang-format on
}

// Convert our Protection enum to Windows protection
DWORD EnumToWinProtection(Protection prot) {
  // clang-format off
  switch (prot) {
    case Protection::kNone:             return PAGE_NOACCESS;
    case Protection::kRead:             return PAGE_READONLY;
    case Protection::kWrite:            return PAGE_READWRITE;
    case Protection::kExecute:          return PAGE_EXECUTE;
    case Protection::kExecuteRead:      return PAGE_EXECUTE_READ;
    case Protection::kExecuteWrite:     return PAGE_EXECUTE_READWRITE;
    case Protection::kReadWrite:        return PAGE_READWRITE;
    case Protection::kExecuteReadWrite: return PAGE_EXECUTE_READWRITE;
    default:
      return PAGE_NOACCESS;
  }
  // clang-format on
}

// Detect architecture from process
Architecture DetectArchitecture(HANDLE process) {
  SYSTEM_INFO sys_info = {};
  ::GetNativeSystemInfo(&sys_info);

  BOOL is_wow64 = FALSE;
  if (process == ::GetCurrentProcess()) {
    ::IsWow64Process(::GetCurrentProcess(), &is_wow64);
  } else {
    ::IsWow64Process(process, &is_wow64);
  }

  if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
    if (is_wow64) {
      return Architecture::kX86;
    }
    return Architecture::kX64;
  }
  // clang-format off
  if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) { return Architecture::kX86; }
  if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM) { return Architecture::kArmV7; }
  if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) { return Architecture::kAArch64; }
  // clang-format on

  return Architecture::kGeneric;
}

// Get process path and name
void GetProcessInfo(HANDLE process, ProcessDescriptor& desc) {
  char path[MAX_PATH] = {};
  DWORD size = MAX_PATH;

  if (::QueryFullProcessImageNameA(process, 0, path, &size)) {
    desc.path = path;
    desc.name = GetFileName(path);
  }
}

// Get process start time
uint64_t GetProcessStartTime(HANDLE process) {
  FILETIME creation;
  FILETIME exit;
  FILETIME kernel;
  FILETIME user;
  if (::GetProcessTimes(process, &creation, &exit, &kernel, &user)) {
    ULARGE_INTEGER time;
    time.LowPart = creation.dwLowDateTime;
    time.HighPart = creation.dwHighDateTime;
    return time.QuadPart / 10000;  // Convert to milliseconds
  }
  return 0;
}

}  // anonymous namespace

// Process API
bool ListProcesses(std::function<bool(const ProcessDescriptor&)> callback) {
  // Build PID → PPID map in one pass
  std::unordered_map<DWORD, DWORD> parent_map;
  HANDLE h_snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (h_snapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32 pe32 = {.dwSize = sizeof(pe32)};
    if (::Process32First(h_snapshot, &pe32)) {
      do {
        parent_map[pe32.th32ProcessID] = pe32.th32ParentProcessID;
      } while (::Process32Next(h_snapshot, &pe32));
    }
    ::CloseHandle(h_snapshot);
  }

  // Enumerate processes
  DWORD processes[1024];
  DWORD cb_needed;
  // We must explicitly call EnumProcessesW to avoid macro issues
  if (!::EnumProcesses(processes, sizeof(processes), &cb_needed)) {
    return false;
  }

  DWORD count = cb_needed / sizeof(DWORD);
  for (DWORD i = 0; i < count; ++i) {
    const DWORD pid = processes[i];
    if (pid == 0) {
      continue;
    }

    ProcessHandle h_process =
        OpenProcessHandle(pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
    if (!h_process) {
      continue;
    }

    ProcessDescriptor desc = {};
    desc.pid = pid;
    desc.ppid = parent_map[pid];  // O(1) lookup
    desc.arch = DetectArchitecture(h_process.get());
    desc.bits =
        (desc.arch == Architecture::kX64 || desc.arch == Architecture::kAArch64)
            ? 64
            : 32;
    desc.start_time = GetProcessStartTime(h_process.get());
    GetProcessInfo(h_process.get(), desc);

    if (!callback(desc)) {
      break;
    }
  }  // h_process is automatically closed here
  return true;
}

ProcessDescriptor GetCurrentProcess() {
  ProcessDescriptor desc = {};
  HANDLE h_current = ::GetCurrentProcess();
  desc.pid = GetCurrentProcessId();
  desc.ppid = 0;
  desc.arch = DetectArchitecture(h_current);
  desc.bits =
      (desc.arch == Architecture::kX64 || desc.arch == Architecture::kAArch64)
          ? 64
          : 32;
  desc.start_time = GetProcessStartTime(h_current);
  GetProcessInfo(h_current, desc);

  return desc;
}

std::optional<ProcessDescriptor> GetProcess(uint32_t pid) {
  ProcessHandle h_process =
      OpenProcessHandle(pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
  if (!h_process) {
    return std::nullopt;
  }

  ProcessDescriptor desc = {};
  desc.pid = pid;
  desc.ppid = 0;
  desc.arch = DetectArchitecture(h_process.get());
  desc.bits =
      (desc.arch == Architecture::kX64 || desc.arch == Architecture::kAArch64)
          ? 64
          : 32;
  desc.start_time = GetProcessStartTime(h_process.get());

  // Get parent PID
  PROCESSENTRY32 pe32 = {};
  pe32.dwSize = sizeof(pe32);
  HANDLE h_snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (h_snapshot != INVALID_HANDLE_VALUE) {
    if (::Process32First(h_snapshot, &pe32)) {
      do {
        if (pe32.th32ProcessID == pid) {
          desc.ppid = pe32.th32ParentProcessID;
          break;
        }
      } while (::Process32Next(h_snapshot, &pe32));
    }
    ::CloseHandle(h_snapshot);
  }

  GetProcessInfo(h_process.get(), desc);

  return desc;
}

std::optional<std::string> GetCommandLine(const ProcessDescriptor& process) {
  ProcessHandle h_process = OpenProcessHandle(
      process.pid, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION);
  if (!h_process) {
    return std::nullopt;
  }

  // Check if it's the current process
  if (process.pid == GetCurrentProcessId()) {
    char* cmdline = ::GetCommandLineA();
    return std::string(cmdline);
  }

  // For remote process, we need to read PEB (complex, return placeholder for
  // now) Real implementation would need to read PEB and ProcessParameters
  return std::nullopt;
}

std::optional<ProcessDescriptor> FindProcess(std::string_view name) {
  std::optional<ProcessDescriptor> found;
  ListProcesses([&found, name](const ProcessDescriptor& process) {
    if (GetFileName(process.path) == name || process.name == name) {
      found = process;
      return false;
    }
    return true;
  });
  return found;
}

bool IsProcessAlive(const ProcessDescriptor& process) {
  ProcessHandle h_process =
      OpenProcessHandle(process.pid, PROCESS_QUERY_INFORMATION);
  if (!h_process) {
    return false;
  }

  DWORD exit_code;
  bool alive = ::GetExitCodeProcess(h_process.get(), &exit_code) &&
               exit_code == STILL_ACTIVE;

  return alive;
}

size_t GetProcessBits() {
  return sizeof(void*) * 8;
}

size_t GetSystemBits() {
  SYSTEM_INFO sys_info = {};
  ::GetNativeSystemInfo(&sys_info);
  return (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
          sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
             ? 64
             : 32;
}

// Thread API

bool EnumThreads(std::function<bool(const ThreadDescriptor&)> callback) {
  return EnumThreads(GetCurrentProcess(), callback);
}

bool EnumThreads(const ProcessDescriptor& process,
                 std::function<bool(const ThreadDescriptor&)> callback) {
  HANDLE h_snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (h_snapshot == INVALID_HANDLE_VALUE) {
    return false;
  }

  THREADENTRY32 te32 = {};
  te32.dwSize = sizeof(te32);

  if (::Thread32First(h_snapshot, &te32)) {
    do {
      if (te32.th32OwnerProcessID == process.pid) {
        ThreadDescriptor desc = {};
        desc.tid = te32.th32ThreadID;
        desc.owner_pid = te32.th32OwnerProcessID;

        if (!callback(desc)) {
          break;
        }
      }
    } while (::Thread32Next(h_snapshot, &te32));
  }

  ::CloseHandle(h_snapshot);
  return true;
}

ThreadDescriptor GetCurrentThread() {
  ThreadDescriptor desc = {};
  desc.tid = GetCurrentThreadId();
  desc.owner_pid = GetCurrentProcessId();
  return desc;
}

std::optional<ThreadDescriptor> GetThread(const ProcessDescriptor& process) {
  std::optional<ThreadDescriptor> found;
  EnumThreads(process, [&found, &process](const ThreadDescriptor& thread) {
    // For simplicity, return the first thread of the process
    found = thread;
    return false;
  });
  return found;
}

std::optional<ProcessDescriptor> GetThreadProcess(
    const ThreadDescriptor& thread) {
  return GetProcess(thread.owner_pid);
}

// Module API

bool EnumModules(std::function<bool(const ModuleDescriptor&)> callback) {
  return EnumModules(GetCurrentProcess(), callback);
}

bool EnumModules(const ProcessDescriptor& process,
                 std::function<bool(const ModuleDescriptor&)> callback) {
  ProcessHandle h_process = OpenProcessHandle(
      process.pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
  if (!h_process) {
    return false;
  }

  HMODULE modules[1024];
  DWORD cb_needed;

  if (::EnumProcessModules(
          h_process.get(), modules, sizeof(modules), &cb_needed)) {
    DWORD count = cb_needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count; i++) {
      ModuleDescriptor desc = {};
      desc.base = reinterpret_cast<uintptr_t>(modules[i]);

      // Get module information
      MODULEINFO mod_info = {};
      if (::GetModuleInformation(
              h_process.get(), modules[i], &mod_info, sizeof(mod_info))) {
        desc.size = mod_info.SizeOfImage;
        desc.end = desc.base + desc.size;
      }

      // Get module path and name
      char path[MAX_PATH];
      DWORD path_len =
          ::GetModuleFileNameExA(h_process.get(), modules[i], path, MAX_PATH);
      if (path_len > 0 && path_len < MAX_PATH) {
        desc.path = path;
        desc.name = GetFileName(path);
      }

      if (!callback(desc)) {
        break;
      }
    }
  }

  return true;
}

std::optional<ModuleDescriptor> FindModule(std::string_view name) {
  return FindModule(GetCurrentProcess(), name);
}

std::optional<ModuleDescriptor> FindModule(const ProcessDescriptor& process,
                                           std::string_view name) {
  std::optional<ModuleDescriptor> found;
  EnumModules(process, [&found, name](const ModuleDescriptor& module) {
    if (GetFileName(module.path) == name || module.name == name) {
      found = module;
      return false;
    }
    return true;
  });
  return found;
}

bool LoadModule(std::string_view path, ModuleDescriptor* module_out) {
  return LoadModule(GetCurrentProcess(), path, module_out);
}

bool LoadModule(const ProcessDescriptor& process,
                std::string_view path,
                ModuleDescriptor* module_out) {
  ProcessHandle h_process = OpenProcessHandle(
      process.pid,
      PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
          PROCESS_VM_WRITE | PROCESS_VM_READ);
  if (!h_process) {
    return false;
  }

  // For simplicity, we'll use LoadLibrary for current process
  if (process.pid == GetCurrentProcessId()) {
    HMODULE h_module = ::LoadLibraryA(path.data());
    if (!h_module) {
      return false;
    }

    if (module_out) {
      module_out->base = reinterpret_cast<uintptr_t>(h_module);
      module_out->size = 0;
      module_out->end = module_out->base;
      module_out->path = path;
      module_out->name = GetFileName(path);
    }
    return true;
  }

  // Remote thread injection would be needed for other processes
  return false;
}

bool UnloadModule(const ModuleDescriptor& module) {
  return UnloadModule(GetCurrentProcess(), module);
}

bool UnloadModule(const ProcessDescriptor& process,
                  const ModuleDescriptor& module) {
  if (process.pid != GetCurrentProcessId()) {
    return false;  // Not supported in this simplified implementation
  }

  HMODULE h_module = std::bit_cast<HMODULE>(module.base);
  return ::FreeLibrary(h_module) != FALSE;
}

// Segment API

bool EnumSegments(std::function<bool(const SegmentDescriptor&)> callback) {
  return EnumSegments(GetCurrentProcess(), callback);
}

bool EnumSegments(const ProcessDescriptor& process,
                  std::function<bool(const SegmentDescriptor&)> callback) {
  ProcessHandle h_process =
      OpenProcessHandle(process.pid, PROCESS_QUERY_INFORMATION);
  if (!h_process) {
    return false;
  }

  // Enumerate memory regions
  MEMORY_BASIC_INFORMATION mbi = {};
  uintptr_t address = 0;

  while (::VirtualQueryEx(h_process.get(),
                          std::bit_cast<LPCVOID>(address),
                          &mbi,
                          sizeof(mbi)) == sizeof(mbi)) {
    if (mbi.State == MEM_COMMIT && mbi.RegionSize > 0) {
      SegmentDescriptor desc = {};
      desc.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
      desc.size = mbi.RegionSize;
      desc.end = desc.base + desc.size;
      desc.protection = WinProtectionToEnum(mbi.Protect);

      if (!callback(desc)) {
        break;
      }
    }

    address += mbi.RegionSize;
    if (address == 0) {
      break;  // Overflow
    }
  }

  return true;
}

std::optional<SegmentDescriptor> FindSegment(uintptr_t address) {
  return FindSegment(GetCurrentProcess(), address);
}

std::optional<SegmentDescriptor> FindSegment(const ProcessDescriptor& process,
                                             uintptr_t address) {
  ProcessHandle h_process =
      OpenProcessHandle(process.pid, PROCESS_QUERY_INFORMATION);
  if (!h_process) {
    return std::nullopt;
  }

  MEMORY_BASIC_INFORMATION mbi = {};
  if (::VirtualQueryEx(h_process.get(),
                       std::bit_cast<LPCVOID>(address),
                       &mbi,
                       sizeof(mbi)) == sizeof(mbi)) {
    if (mbi.State == MEM_COMMIT) {
      SegmentDescriptor desc = {};
      desc.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
      desc.size = mbi.RegionSize;
      desc.end = desc.base + desc.size;
      desc.protection = WinProtectionToEnum(mbi.Protect);
      return desc;
    }
  }

  return std::nullopt;
}

// Memory API

size_t ReadMemory(uintptr_t source, std::span<std::byte> dest) {
  if (source == 0 || dest.empty()) {
    return 0;
  }
  return ReadMemory(GetCurrentProcess(), source, dest);
}

size_t ReadMemory(const ProcessDescriptor& process,
                  uintptr_t source,
                  std::span<std::byte> dest) {
  if (source == 0 || dest.empty()) {
    return 0;
  }

  ProcessHandle h_process = OpenProcessHandle(process.pid, PROCESS_VM_READ);
  if (!h_process) {
    return 0;
  }

  SIZE_T bytes_read = 0;
  bool success = ::ReadProcessMemory(h_process.get(),
                                     std::bit_cast<LPCVOID>(source),
                                     dest.data(),
                                     dest.size(),
                                     &bytes_read) != FALSE;

  return success ? bytes_read : 0;
}

size_t WriteMemory(uintptr_t dest, std::span<const std::byte> source) {
  if (dest == 0 || source.empty()) {
    return 0;
  }
  return WriteMemory(GetCurrentProcess(), dest, source);
}

size_t WriteMemory(const ProcessDescriptor& process,
                   uintptr_t dest,
                   std::span<const std::byte> source) {
  if (dest == 0 || source.empty()) {
    return 0;
  }

  ProcessHandle h_process =
      OpenProcessHandle(process.pid, PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
  if (!h_process) {
    return 0;
  }

  SIZE_T bytes_written = 0;
  bool success = ::WriteProcessMemory(h_process.get(),
                                      std::bit_cast<LPVOID>(dest),
                                      source.data(),
                                      source.size(),
                                      &bytes_written) != FALSE;

  return success ? bytes_written : 0;
}

size_t MemoryFill(uintptr_t dest, std::byte value, size_t size) {
  return MemoryFill(GetCurrentProcess(), dest, value, size);
}

size_t MemoryFill(const ProcessDescriptor& process,
                  uintptr_t dest,
                  std::byte value,
                  size_t size) {
  if (dest == 0 || size == 0) {
    return 0;
  }

  // Create a small, fixed-size chunk
  constexpr size_t kChunkSize = 4096;
  std::vector<std::byte> chunk(kChunkSize, value);

  size_t total_written = 0;
  while (total_written < size) {
    // Calculate how much to write in this iteration
    size_t bytes_to_write = std::min(kChunkSize, size - total_written);
    std::span<const std::byte> write_span = {chunk.data(), bytes_to_write};

    size_t bytes_written =
        WriteMemory(process, dest + total_written, write_span);

    total_written += bytes_written;

    // If we didn't write the full chunk, a write error occurred
    if (bytes_written != bytes_to_write) {
      break;
    }
  }
  return total_written;
}

bool ProtectMemory(uintptr_t address,
                   size_t size,
                   Protection prot,
                   Protection* old_prot) {
  return ProtectMemory(GetCurrentProcess(), address, size, prot, old_prot);
}

bool ProtectMemory(const ProcessDescriptor& process,
                   uintptr_t address,
                   size_t size,
                   Protection prot,
                   Protection* old_prot) {
  ProcessHandle h_process =
      OpenProcessHandle(process.pid, PROCESS_VM_OPERATION);
  if (!h_process) {
    return false;
  }

  DWORD old_protect = 0;
  bool success = ::VirtualProtectEx(h_process.get(),
                                    std::bit_cast<LPVOID>(address),
                                    size,
                                    EnumToWinProtection(prot),
                                    &old_protect) != FALSE;

  if (old_prot && success) {
    *old_prot = WinProtectionToEnum(old_protect);
  }

  return success;
}  // h_process is automatically closed here

std::optional<ProcessAddress> AllocateMemory(ProcessSize size,
                                             Protection prot) {
  return AllocateMemory(GetCurrentProcess(), size, prot);
}

std::optional<ProcessAddress> AllocateMemory(const ProcessDescriptor& process,
                                             ProcessSize size,
                                             Protection prot) {
  ProcessHandle h_process = OpenProcessHandle(
      process.pid, PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE);
  if (!h_process) {
    return std::nullopt;
  }

  LPVOID address = ::VirtualAllocEx(h_process.get(),
                                    nullptr,
                                    size,
                                    MEM_COMMIT | MEM_RESERVE,
                                    EnumToWinProtection(prot));

  if (!address) {
    return std::nullopt;
  }
  return std::bit_cast<uintptr_t>(address);
}

bool FreeMemory(uintptr_t address, size_t size) {
  return FreeMemory(GetCurrentProcess(), address, size);
}

bool FreeMemory(const ProcessDescriptor& process,
                uintptr_t address,
                size_t size) {
  ProcessHandle h_process =
      OpenProcessHandle(process.pid, PROCESS_VM_OPERATION);
  if (!h_process) {
    return false;
  }

  // For VirtualFreeEx with MEM_RELEASE, the size must be 0.
  // The address must be the base address from VirtualAllocEx.
  bool success =
      ::VirtualFreeEx(
          h_process.get(), std::bit_cast<LPVOID>(address), 0, MEM_RELEASE) !=
      FALSE;

  return success;
}

std::optional<ProcessAddress> ResolvePointerPath(
    ProcessAddress base_address, std::span<const ProcessAddress> offsets) {
  return ResolvePointerPath(GetCurrentProcess(), base_address, offsets);
}

std::optional<ProcessAddress> ResolvePointerPath(
    const ProcessDescriptor& process,
    ProcessAddress base_address,
    std::span<const ProcessAddress> offsets) {
  ProcessAddress current = base_address;

  for (size_t i = 0; i < offsets.size(); i++) {
    if (i > 0) {
      // Read pointer sized for the TARGET process.
      if (process.bits == 64) {
        uint64_t ptr_value = 0;
        if (ReadMemory(process,
                       current,
                       std::span<std::byte>(
                           reinterpret_cast<std::byte*>(&ptr_value), 8)) != 8) {
          return std::nullopt;
        }
        current = ptr_value;
      } else {
        uint32_t ptr_value = 0;
        if (ReadMemory(process,
                       current,
                       std::span<std::byte>(
                           reinterpret_cast<std::byte*>(&ptr_value), 4)) != 4) {
          return std::nullopt;
        }
        current = ptr_value;
      }
      if (current == 0) {
        return std::nullopt;
      }
    }

    current += offsets[i];
  }

  return current;
}

// Scan API

std::optional<uintptr_t> ScanData(std::span<const std::byte> data,
                                  uintptr_t address,
                                  size_t scan_size) {
  return ScanData(GetCurrentProcess(), data, address, scan_size);
}

std::optional<uintptr_t> ScanData(const ProcessDescriptor& process,
                                  std::span<const std::byte> data,
                                  uintptr_t address,
                                  size_t scan_size) {
  if (data.empty() || scan_size == 0) {
    return std::nullopt;
  }

  std::vector<std::byte> buffer(scan_size);
  size_t bytes_read = ReadMemory(process, address, buffer);
  if (bytes_read < data.size()) {
    return std::nullopt;
  }

  // Adjust search range to avoid reading past the buffer.
  auto end_it = buffer.begin() + bytes_read;
  if (bytes_read >= data.size()) {
    end_it = buffer.begin() + bytes_read - data.size() + 1;
  }

  auto it = std::search(buffer.begin(), end_it, data.begin(), data.end());

  if (it != end_it) {
    size_t offset = std::distance(buffer.begin(), it);
    return address + offset;
  }

  return std::nullopt;
}

std::optional<uintptr_t> ScanPattern(std::span<const std::byte> pattern,
                                     std::string_view mask,
                                     uintptr_t address,
                                     size_t scan_size) {
  return ScanPattern(GetCurrentProcess(), pattern, mask, address, scan_size);
}

std::optional<uintptr_t> ScanPattern(const ProcessDescriptor& process,
                                     std::span<const std::byte> pattern,
                                     std::string_view mask,
                                     uintptr_t address,
                                     size_t scan_size) {
  if (pattern.size() != mask.size() || scan_size == 0) {
    return std::nullopt;
  }

  std::vector<std::byte> buffer(scan_size);
  size_t bytes_read = ReadMemory(process, address, buffer);
  if (bytes_read < pattern.size()) {
    return std::nullopt;
  }

  for (size_t i = 0; i <= bytes_read - pattern.size(); i++) {
    bool match = true;
    for (size_t j = 0; j < pattern.size(); j++) {
      if (mask[j] == 'x' && buffer[i + j] != pattern[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return address + i;
    }
  }

  return std::nullopt;
}

std::optional<uintptr_t> ScanSignature(std::string_view signature,
                                       uintptr_t address,
                                       size_t scan_size) {
  return ScanSignature(GetCurrentProcess(), signature, address, scan_size);
}

std::optional<uintptr_t> ScanSignature(const ProcessDescriptor& process,
                                       std::string_view signature,
                                       uintptr_t address,
                                       size_t scan_size) {
  // Parse signature like "DE AD BE EF ?? ?? 13 37"
  std::vector<std::byte> pattern;
  std::string mask;

  for (size_t i = 0; i < signature.size();) {
    std::string_view byte_str = signature.substr(i, 2);
    if (byte_str == "??") {
      pattern.push_back(std::byte{0});
      mask.push_back('?');
      i += 2;
    } else if (byte_str.size() == 2) {
      char* endptr;
      char buf[3] = {byte_str[0], byte_str[1], '\0'};
      int value = std::strtol(buf, &endptr, 16);
      if (*endptr == '\0') {
        pattern.push_back(static_cast<std::byte>(value));
        mask.push_back('x');
      }
      i += 2;
    } else {
      // Skip invalid characters.
      ++i;
    }
    // Skip spaces.
    if (i < signature.size() && signature[i] == ' ') {
      ++i;
    }
  }

  return ScanPattern(process, pattern, mask, address, scan_size);
}

// Architecture detection
Architecture GetArchitecture() {
  return DetectArchitecture(::GetCurrentProcess());
}

}  // namespace maia::mmem
