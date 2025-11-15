// Copyright (c) Maia

#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "maia/core/memory_protection.h"

namespace maia {

using MemoryAddress = uintptr_t;
using MemoryPtr = void*;
using Pid = uint32_t;
using ProcessHandle = void*;

using Byte = std::byte;

template <typename T>
concept CFundamentalType = std::is_fundamental_v<std::decay_t<T>>;

template <CFundamentalType T>
std::span<Byte> ToBytesView(T& data) {
  return std::span<Byte>(std::bit_cast<Byte*>(&data), sizeof(T));
}

template <CFundamentalType T>
T BytesToFundamentalType(std::span<const Byte> view) {
  return *std::bit_cast<T*>(view.data());
}

struct MemoryRegion {
  uintptr_t base_address{};
  size_t size{};
  MemoryProtection protection_flags;  // e.g., PAGE_READWRITE
};

struct ProcessInfo {
  std::string name;
  Pid pid;
};

// struct Page {
//   MemoryPtr address;
//   size_t size;
// };

// Stores addresses and raw bytes of values from the *previous* scan.
// This is the only thing needed for "changed/unchanged" comparisons.
struct MemorySnapshot {
  std::vector<uintptr_t> addresses;

  // For fixed-size types: contiguous bytes.
  // For strings/bytes: concatenated with sizes tracked separately.
  std::vector<std::byte> values;

  // Only used for variable-length types (string, bytearray).
  std::vector<size_t> sizes;
};

}  // namespace maia
