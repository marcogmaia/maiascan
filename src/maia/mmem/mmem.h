// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace maia::mmem {

// Memory protection flags for virtual memory pages.
enum class Protection : uint32_t {
  kNone = 0,
  kRead = (1 << 0),     // Read access
  kWrite = (1 << 1),    // Write access
  kExecute = (1 << 2),  // Execute access
  kXR = kExecute | kRead,
  kXW = kExecute | kWrite,
  kRW = kRead | kWrite,
  kXRW = kExecute | kRead | kWrite,
};

// Processor architecture types detected in target processes.
enum class Architecture : uint32_t {
  kGeneric = 0,
  kX86,      // 32-bit x86
  kX64,      // 64-bit x86-64
  kArmV7,    // 32-bit ARM
  kAArch64,  // 64-bit ARM
  kMax
};

// Architecture-safe address type (always 64-bit for cross-process
// compatibility).
using ProcessAddress = uint64_t;

// Architecture-safe size type (always 64-bit).
using ProcessSize = uint64_t;

// Information about a running process.
struct ProcessDescriptor {
  uint32_t pid;         // Process identifier
  uint32_t ppid;        // Parent process identifier
  Architecture arch;    // Detected architecture
  size_t bits;          // Pointer width (32 or 64)
  uint64_t start_time;  // Process start time in milliseconds since boot
  std::string path;     // Full executable path
  std::string name;     // Executable filename
};

// Information about a running thread.
struct ThreadDescriptor {
  uint32_t tid;        // Thread identifier
  uint32_t owner_pid;  // Owning process identifier
};

// Information about a loaded module/DLL.
struct ModuleDescriptor {
  uintptr_t base;    // Module base address
  uintptr_t end;     // Module end address
  size_t size;       // Module size in bytes
  std::string path;  // Full module path
  std::string name;  // Module filename
};

// Information about a memory segment.
struct SegmentDescriptor {
  uintptr_t base;         // Segment base address
  uintptr_t end;          // Segment end address
  size_t size;            // Segment size in bytes
  Protection protection;  // Memory protection flags
};

// clang-format off

// Process API

// Enumerates all processes on the system, calling callback for each.
// @param callback Return true to continue enumeration, false to stop
// @return true if enumeration succeeded, false otherwise
bool ListProcesses(std::function<bool(const ProcessDescriptor&)> callback);

// Gets the current process descriptor.
ProcessDescriptor GetCurrentProcess();

// Gets a process descriptor by PID.
std::optional<ProcessDescriptor> GetProcess(uint32_t pid);

// Gets the command line string for a process (returns nullopt for remote processes).
std::optional<std::string> GetCommandLine(const ProcessDescriptor& process);

// Finds a process by executable name or path.
std::optional<ProcessDescriptor> FindProcess(std::string_view name);

// Checks if a process is still running.
bool IsProcessAlive(const ProcessDescriptor& process);

// Gets the pointer width of the current process (32 or 64).
size_t GetProcessBits();

// Gets the pointer width of the system (32 or 64).
size_t GetSystemBits();

// Thread API

// Enumerates threads in the current process.
bool EnumThreads(std::function<bool(const ThreadDescriptor&)> callback);

// Enumerates threads in a specific process.
bool EnumThreads(const ProcessDescriptor& process, std::function<bool(const ThreadDescriptor&)> callback);

// Gets the current thread descriptor.
ThreadDescriptor GetCurrentThread();

// Gets the first thread of a process.
std::optional<ThreadDescriptor> GetThread(const ProcessDescriptor& process);

// Gets the process descriptor for a thread's owner.
std::optional<ProcessDescriptor> GetThreadProcess(const ThreadDescriptor& thread);

// Module API

// Enumerates modules in the current process.
bool EnumModules(std::function<bool(const ModuleDescriptor&)> callback);

// Enumerates modules in a specific process.
bool EnumModules(const ProcessDescriptor& process, std::function<bool(const ModuleDescriptor&)> callback);

// Finds a module by name in the current process.
std::optional<ModuleDescriptor> FindModule(std::string_view name);

// Finds a module by name in a specific process.
std::optional<ModuleDescriptor> FindModule(const ProcessDescriptor& process, std::string_view name);

// Loads a module into the current process.
bool LoadModule(std::string_view path, ModuleDescriptor* module_out = nullptr);

// Loads a module into a specific process (current process only).
bool LoadModule(const ProcessDescriptor& process, std::string_view path, ModuleDescriptor* module_out = nullptr);

// Unloads a module from the current process.
bool UnloadModule(const ModuleDescriptor& module);

// Unloads a module from a specific process (current process only).
bool UnloadModule(const ProcessDescriptor& process, const ModuleDescriptor& module);

// Segment API

// Enumerates memory segments in the current process.
bool EnumSegments(std::function<bool(const SegmentDescriptor&)> callback);

// Enumerates memory segments in a specific process.
bool EnumSegments(const ProcessDescriptor& process, std::function<bool(const SegmentDescriptor&)> callback);

// Finds the segment containing an address in the current process.
std::optional<SegmentDescriptor> FindSegment(uintptr_t address);

// Finds the segment containing an address in a specific process.
std::optional<SegmentDescriptor> FindSegment(const ProcessDescriptor& process, uintptr_t address);

// Memory API

// Reads memory from the current process. Returns bytes read.
size_t ReadMemory(uintptr_t source, std::span<std::byte> dest);

// Reads memory from a process. Returns bytes read.
size_t ReadMemory(const ProcessDescriptor& process, uintptr_t source, std::span<std::byte> dest);

// Writes memory to the current process. Returns bytes written.
size_t WriteMemory(uintptr_t dest, std::span<const std::byte> source);

// Writes memory to a process. Returns bytes written.
size_t WriteMemory(const ProcessDescriptor& process, uintptr_t dest, std::span<const std::byte> source);

// Fills memory with a byte value in the current process. Returns bytes set.
size_t MemoryFill(uintptr_t dest, std::byte value, size_t size);

// Fills memory with a byte value in a process. Returns bytes set.
size_t MemoryFill(const ProcessDescriptor& process, uintptr_t dest, std::byte value, size_t size);

// Changes memory protection in the current process.
bool ProtectMemory(uintptr_t address, size_t size, Protection prot, Protection* old_prot = nullptr);

// Changes memory protection in a process.
bool ProtectMemory(const ProcessDescriptor& process, uintptr_t address, size_t size, Protection prot, Protection* old_prot = nullptr);

// Allocates memory in the current process.
std::optional<ProcessAddress> AllocateMemory(ProcessSize size, Protection prot);

// Allocates memory in a process.
std::optional<ProcessAddress> AllocateMemory(const ProcessDescriptor& process, ProcessSize size, Protection prot);

// Frees allocated memory in the current process.
bool FreeMemory(uintptr_t address, size_t size);

// Frees allocated memory in a process.
bool FreeMemory(const ProcessDescriptor& process, uintptr_t address, size_t size);

// Resolves a pointer path through multiple dereferences and offsets.
std::optional<ProcessAddress> ResolvePointerPath(ProcessAddress base_address, std::span<const ProcessAddress> offsets);

// Resolves a pointer path in a target process (handles 32/64-bit correctly).
std::optional<ProcessAddress> ResolvePointerPath(const ProcessDescriptor& process, ProcessAddress base_address, std::span<const ProcessAddress> offsets);

// Scan API

// Scans for raw data in the current process.
std::optional<uintptr_t> ScanData(std::span<const std::byte> data, uintptr_t address, size_t scan_size);

// Scans for raw data in a process.
std::optional<uintptr_t> ScanData(const ProcessDescriptor& process, std::span<const std::byte> data, uintptr_t address, size_t scan_size);

// Scans for pattern/mask in the current process.
std::optional<uintptr_t> ScanPattern(std::span<const std::byte> pattern, std::string_view mask, uintptr_t address, size_t scan_size);

// Scans for pattern/mask in a process.
std::optional<uintptr_t> ScanPattern(const ProcessDescriptor& process, std::span<const std::byte> pattern, std::string_view mask, uintptr_t address, size_t scan_size);

// Scans for hexadecimal signature string (e.g., "DE AD BE EF ?? ?? 13 37") in the current process.
std::optional<uintptr_t> ScanSignature(std::string_view signature, uintptr_t address, size_t scan_size);

// Scans for hexadecimal signature string in a process.
std::optional<uintptr_t> ScanSignature(const ProcessDescriptor& process, std::string_view signature, uintptr_t address, size_t scan_size);

// Architecture detection

// Gets the architecture of the current process.
Architecture GetArchitecture();

// clang-format on

}  // namespace maia::mmem
