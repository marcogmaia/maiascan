// Copyright (c) Maia

#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string>

namespace maia {

using MemoryPtr = void*;
using MemoryAddress = uintptr_t;
using Pid = uint32_t;

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
  // TODO: Add a constructor to simplify the creation.

  size_t base_address;

  // View over the entire memory region.
  std::span<std::byte> view;

  uint32_t protection_flags = 0;  // e.g., PAGE_READWRITE
  uint32_t state = 0;             // e.g., MEM_COMMIT

  void* data() const {
    return view.data();
  }

  size_t size() const {
    return view.size();
  }
};

struct ProcessInfo {
  std::string name;
  Pid pid;
};

struct Page {
  MemoryPtr address;
  size_t size;
};

}  // namespace maia
