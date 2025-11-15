// Copyright (c) Maia

#pragma once

// Platform-specific includes for protection constants
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

#include <cstdint>

namespace maia {

// Cross-platform memory protection flags
// These values are chosen to be compatible with both Windows and Linux
// protection schemes
enum class MemoryProtection : uint32_t {
  kNone = 0x00,
  kRead = 0x01,
  kWrite = 0x02,
  kExecute = 0x04,

  kReadWrite = kRead | kWrite,
  kReadExecute = kRead | kExecute,
  kReadWriteExecute = kRead | kWrite | kExecute,

  // Platform-specific flags that can be OR'd with the above
  kGuard = 0x100,         // Guard page (Windows PAGE_GUARD)
  kNoCache = 0x200,       // No cache (Windows PAGE_NOCACHE)
  kWriteCombine = 0x400,  // Write combining (Windows PAGE_WRITECOMBINE)
};

// Helper functions to check memory protection flags
inline bool IsReadable(uint32_t protection_flags) {
  return (protection_flags & static_cast<uint32_t>(MemoryProtection::kRead)) !=
         0;
}

inline bool IsWritable(uint32_t protection_flags) {
  return (protection_flags & static_cast<uint32_t>(MemoryProtection::kWrite)) !=
         0;
}

inline bool IsExecutable(uint32_t protection_flags) {
  return (protection_flags &
          static_cast<uint32_t>(MemoryProtection::kExecute)) != 0;
}

inline bool IsAccessible(uint32_t protection_flags) {
  return protection_flags != static_cast<uint32_t>(MemoryProtection::kNone);
}

inline bool IsGuardPage(uint32_t protection_flags) {
  return (protection_flags & static_cast<uint32_t>(MemoryProtection::kGuard)) !=
         0;
}

// Platform-specific conversion functions
namespace detail {

// Convert Windows protection flags to cross-platform format
inline uint32_t WindowsProtectionToCrossPlatform(uint32_t windows_protect) {
  uint32_t result = static_cast<uint32_t>(MemoryProtection::kNone);

  // Check for no access
  if (windows_protect & PAGE_NOACCESS) {
    return result;
  }

  // Check read permission
  if (windows_protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE)) {
    result |= static_cast<uint32_t>(MemoryProtection::kRead);
  }

  // Check write permission
  if (windows_protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE |
                         PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) {
    result |= static_cast<uint32_t>(MemoryProtection::kWrite);
  }

  // Check execute permission
  if (windows_protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) {
    result |= static_cast<uint32_t>(MemoryProtection::kExecute);
  }

  // Check for guard page
  if (windows_protect & PAGE_GUARD) {
    result |= static_cast<uint32_t>(MemoryProtection::kGuard);
  }

  // Check for no cache
  if (windows_protect & PAGE_NOCACHE) {
    result |= static_cast<uint32_t>(MemoryProtection::kNoCache);
  }

  // Check for write combining
  if (windows_protect & PAGE_WRITECOMBINE) {
    result |= static_cast<uint32_t>(MemoryProtection::kWriteCombine);
  }

  return result;
}

// // Convert Linux protection flags to cross-platform format
// inline uint32_t LinuxProtectionToCrossPlatform(int linux_prot) {
//   uint32_t result = static_cast<uint32_t>(MemoryProtection::None);

//   if (linux_prot & PROT_READ) {
//     result |= static_cast<uint32_t>(MemoryProtection::Read);
//   }
//   if (linux_prot & PROT_WRITE) {
//     result |= static_cast<uint32_t>(MemoryProtection::Write);
//   }
//   if (linux_prot & PROT_EXEC) {
//     result |= static_cast<uint32_t>(MemoryProtection::Execute);
//   }

//   return result;
// }

}  // namespace detail

}  // namespace maia
