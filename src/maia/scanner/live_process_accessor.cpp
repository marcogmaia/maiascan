// Copyright (c) Maia

#include <Windows.h>

#include <Psapi.h>
#include <TlHelp32.h>

#include "maia/scanner/live_process_accessor.h"

#include <vector>

#include "maia/core/memory_common.h"
#include "maia/core/memory_protection.h"
#include "maia/logging.h"

namespace maia::scanner {

ProcessHandle OpenHandle(uint32_t pid) {
  HANDLE handle =
      OpenProcess(PROCESS_QUERY_INFORMATION |  // Required for VirtualQueryEx
                      PROCESS_VM_READ |        // Required for ReadProcessMemory
                      PROCESS_VM_WRITE |      // Required for WriteProcessMemory
                      PROCESS_VM_OPERATION |  // Required for VirtualProtectEx
                      SYNCHRONIZE,  // Required for WaitForSingleObject
                  FALSE,
                  pid);
  return handle;
}

namespace {

MemoryAddress GetBaseAddress(Pid pid) {
  auto* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return {};
  }
  MODULEENTRY32 mod_entry{.dwSize = sizeof(MODULEENTRY32)};
  bool success = Module32First(snapshot, &mod_entry) != 0;
  if (!success) {
    auto err = GetLastError();
    return {};
  }
  return std::bit_cast<MemoryAddress>(mod_entry.modBaseAddr);
}

MemoryPtr ToPtr(uintptr_t ptr) {
  return std::bit_cast<MemoryPtr>(ptr);
}

uintptr_t GetProcessBaseAddress(uint32_t process_id) {
  MemoryAddress base_address = 0;
  HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);

  if (snapshot != INVALID_HANDLE_VALUE) {
    MODULEENTRY32 module_entry;
    module_entry.dwSize = sizeof(MODULEENTRY32);

    // Module32First retrieves information about the first module (the .exe)
    if (Module32First(snapshot, &module_entry)) {
      base_address = reinterpret_cast<MemoryAddress>(module_entry.modBaseAddr);
    }

    CloseHandle(snapshot);
  }

  return base_address;
}

}  // namespace

std::vector<MemoryRegion> LiveProcessAccessor::GetMemoryRegions() const {
  std::vector<MemoryRegion> regions;
  if (!IsProcessValid()) {
    return regions;
  }

  // Get native system info to find the true address space.
  SYSTEM_INFO system_info;
  ::GetNativeSystemInfo(&system_info);

  uintptr_t current_address =
      std::bit_cast<uintptr_t>(system_info.lpMinimumApplicationAddress);
  const uintptr_t max_address =
      std::bit_cast<uintptr_t>(system_info.lpMaximumApplicationAddress);

  MEMORY_BASIC_INFORMATION mbi;

  // Loop through the valid address space.
  while (current_address < max_address &&
         VirtualQueryEx(handle_,
                        std::bit_cast<const void*>(current_address),
                        &mbi,
                        sizeof(mbi)) != 0) {
    // Convert Windows protection flags to cross-platform format.
    uint32_t protection_flags =
        detail::WindowsProtectionToCrossPlatform(mbi.Protect);

    bool is_desired_page_block = (mbi.State == MEM_COMMIT) &&
                                 IsAccessible(protection_flags) &&
                                 !IsGuardPage(protection_flags);

    if (is_desired_page_block) {
      regions.emplace_back(MemoryRegion{
          .base_address = std::bit_cast<uintptr_t>(mbi.BaseAddress),
          .size = mbi.RegionSize,
          .protection_flags = protection_flags});
    }

    // Move to the next region
    // The overflow check is still good practice, even with max_address.
    uintptr_t next_address =
        std::bit_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

    if (next_address <= current_address) {
      break;  // Address space wrapped
    }

    current_address = next_address;
  }

  return regions;
}

bool LiveProcessAccessor::ReadMemory(uintptr_t address,
                                     std::span<std::byte> buffer) const {
  if (buffer.empty()) {
    return true;
  }

  MemoryPtr address_ptr = ToPtr(address);

  size_t bytes_read = 0;
  bool result = ReadProcessMemory(
      handle_, address_ptr, buffer.data(), buffer.size(), &bytes_read);

  // We must check both that the API call succeeded (result != 0)
  // AND that we read the exact number of bytes we requested.
  return (result != 0) && (bytes_read == buffer.size());
}

bool LiveProcessAccessor::WriteMemory(uintptr_t address,
                                      std::span<const std::byte> buffer) {
  if (buffer.empty()) {
    return true;
  }

  DWORD old_protection_flag = 0;

  // Change the memory protection to be writable. PAGE_EXECUTE_READWRITE is used
  // to cover all bases (e.g., modifying .text section). Request write access.
  auto* address_ptr = ToPtr(address);
  BOOL protect_result = VirtualProtectEx(handle_,
                                         address_ptr,
                                         buffer.size(),
                                         PAGE_EXECUTE_READWRITE,
                                         &old_protection_flag);

  if (protect_result == 0) {
    // Failed to change permissions. We can still try to write, as the page
    // might have already been writable.
    LogWarning("Failed to change the protection of virtual page. Error: {}",
               GetLastError());
  }

  // Write the buffer.
  size_t bytes_written = 0;
  BOOL write_result = WriteProcessMemory(
      handle_, address_ptr, buffer.data(), buffer.size(), &bytes_written);

  // (Important) Restore the original memory permissions
  if (protect_result != 0) {
    DWORD temp;  // Dummy variable to satisfy the API
    VirtualProtectEx(
        handle_, address_ptr, buffer.size(), old_protection_flag, &temp);
  }

  // Success is defined as the write API call succeeding AND
  // all requested bytes being written.
  return (write_result != 0) && (bytes_written == buffer.size());
}

LiveProcessAccessor::LiveProcessAccessor(ProcessHandle handle)
    : handle_(handle),
      process_id_(::GetProcessId(handle)),
      process_base_address_(GetProcessBaseAddress(process_id_)) {}

bool LiveProcessAccessor::IsProcessValid() const {
  // Check if the handle is null or invalid, which can happen
  // if the process was never opened successfully or was closed.
  if (handle_ == nullptr || handle_ == INVALID_HANDLE_VALUE) {
    return false;
  }

  // Check the process handle with a 0ms timeout.
  DWORD wait_result = ::WaitForSingleObject(handle_, 0);

  // If the wait times out, it means the object (the process) is
  // *not* signaled, which means it is still running.
  return (wait_result == WAIT_TIMEOUT);
}

uint32_t LiveProcessAccessor::GetProcessId() const {
  return process_id_;
}

std::string LiveProcessAccessor::GetProcessName() const {
  if (!IsProcessValid()) {
    return "";
  }

  // MAX_PATH is typically 260 chars.
  wchar_t process_name[MAX_PATH];

  // The 'W' stands for the Wide-character (Unicode) version.
  DWORD chars_written =
      ::GetModuleBaseNameW(handle_,       // The process handle
                           nullptr,       // Get the main executable
                           process_name,  // The buffer to write to
                           MAX_PATH       // The size of the buffer
      );

  if (chars_written == 0) {
    // The call failed (e.g., access denied, or process closing)
    return "";
  }

  // First, find the required buffer size for the UTF-8 string.
  int size_needed = ::WideCharToMultiByte(
      CP_UTF8,                          // Convert to UTF-8
      0,                                // Default flags
      process_name,                     // The source (UTF-16) string
      static_cast<int>(chars_written),  // The length of the source string
      nullptr,                          // NULL to calculate buffer size
      0,                                // 0 to calculate buffer size
      nullptr,                          // No default char
      nullptr                           // No flag
  );

  if (size_needed == 0) {
    // Conversion failed
    return "";
  }

  // Create a std::string and perform the conversion.
  std::string process_name_str(size_needed, 0);
  ::WideCharToMultiByte(CP_UTF8,
                        0,
                        process_name,
                        static_cast<int>(chars_written),
                        process_name_str.data(),  // Target buffer
                        size_needed,              // Target buffer size
                        nullptr,
                        nullptr);

  return process_name_str;
}

uintptr_t LiveProcessAccessor::GetBaseAddress() const {
  return process_base_address_;
}

}  // namespace maia::scanner
