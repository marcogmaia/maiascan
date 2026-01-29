// Copyright (c) Maia

#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string>

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

struct ProcessInfo {
  std::string name;
  Pid pid;
};

}  // namespace maia
