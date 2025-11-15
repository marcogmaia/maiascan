// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <cstdint>

namespace maia::mmem {

enum class Protection : uint32_t {
  kRead = (1 << 0),
  kWrite = (1 << 1),
  kExecute = (1 << 2),
  kXR = kExecute | kRead,
  kXW = kExecute | kWrite,
  kRW = kRead | kWrite,
  kXRW = kExecute | kRead | kWrite,
};

constexpr size_t kMaxPath = 4096;

struct ProcessDescriptor {
  uint32_t pid;
  uint32_t ppid;
  uint32_t arch;
  size_t bits;
  // Process start timestamp, in milliseconds since last boot.
  uint64_t start_time;
  char path[kMaxPath];
  char name[kMaxPath];
};

size_t LM_ReadMemoryEx(const ProcessDescriptor* process,
                       uintptr_t source,
                       std::byte* dest,
                       size_t size);

}  // namespace maia::mmem
