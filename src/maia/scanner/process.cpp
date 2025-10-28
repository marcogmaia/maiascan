// Copyright (c) Maia

#include <Windows.h>

#include <Psapi.h>
#include <TlHelp32.h>

#include "maia/scanner/process.h"

#include <vector>

#include "maia/logging.h"
#include "maia/scanner/memory_common.h"

namespace maia::scanner {

std::vector<MemoryRegion> LiveProcessAccessor::GetMemoryRegions() {
  std::vector<MemoryRegion> regions;
  MEMORY_BASIC_INFORMATION mbi;

  // Start at the lowest possible address
  uintptr_t current_address = 0;

  // Loop using VirtualQueryEx to walk the process's address space
  while (VirtualQueryEx(handle_,
                        std::bit_cast<const void*>(current_address),
                        &mbi,
                        sizeof(mbi)) != 0) {
    // We only care about memory that is actually committed (allocated)
    // and not marked as PAGE_NOACCESS or PAGE_GUARD.
    bool is_desired_page_block = mbi.State == MEM_COMMIT &&
                                 //  mbi.Type == MEM_PRIVATE &&
                                 !(mbi.Protect & PAGE_NOACCESS) &&
                                 !(mbi.Protect & PAGE_GUARD);
    if (is_desired_page_block) {
      regions.emplace_back(MemoryRegion{
          .base_address = std::bit_cast<uintptr_t>(mbi.BaseAddress),
          .size = mbi.RegionSize,
          .protection_flags = mbi.Protect});
    }

    // Move to the next region
    uintptr_t next_address =
        std::bit_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

    // Check for overflow (we've reached the end of the address space).
    if (next_address <= current_address) {
      break;
    }
    current_address = next_address;
  }

  return regions;
}

bool LiveProcessAccessor::ReadMemory(MemoryPtr address,
                                     std::span<std::byte> buffer) {
  if (buffer.empty()) {
    return true;
  }

  size_t bytes_read = 0;
  bool result = ReadProcessMemory(
      handle_, address, buffer.data(), buffer.size(), &bytes_read);

  // We must check both that the API call succeeded (result != 0)
  // AND that we read the exact number of bytes we requested.
  return (result != 0) && (bytes_read == buffer.size());
}

bool LiveProcessAccessor::WriteMemory(MemoryPtr address,
                                      std::span<const std::byte> data) {
  if (data.empty()) {
    return true;
  }

  DWORD old_protection_flag = 0;

  // Change the memory protection to be writable. PAGE_EXECUTE_READWRITE is used
  // to cover all bases (e.g., modifying .text section). Request write access.
  BOOL protect_result = VirtualProtectEx(handle_,
                                         address,
                                         data.size(),
                                         PAGE_EXECUTE_READWRITE,
                                         &old_protection_flag);

  if (protect_result == 0) {
    // Failed to change permissions. We can still try to write, as the page
    // might have already been writable.
    LogWarning("Failed to change the protection of virtual page. Error: {}",
               GetLastError());
  }

  // Write the data.
  size_t bytes_written = 0;
  BOOL write_result = WriteProcessMemory(
      handle_, address, data.data(), data.size(), &bytes_written);

  // (Important) Restore the original memory permissions
  if (protect_result != 0) {
    DWORD temp;  // Dummy variable to satisfy the API
    VirtualProtectEx(handle_, address, data.size(), old_protection_flag, &temp);
  }

  // Success is defined as the write API call succeeding AND
  // all requested bytes being written.
  return (write_result != 0) && (bytes_written == data.size());
}

}  // namespace maia::scanner
