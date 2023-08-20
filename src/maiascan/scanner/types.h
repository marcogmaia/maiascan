
#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace maia {

using MemoryAddress = void *;
using Pid = uint32_t;

using Bytes = std::vector<std::byte>;
using BytesView = std::span<std::byte>;
using BytesViewReadOnly = std::span<const std::byte>;

template <typename T>
concept CFundamentalType = std::is_fundamental_v<std::decay_t<T>>;

template <CFundamentalType T>
BytesView ToBytesView(const T &data) {
  return BytesView(std::bit_cast<std::byte *>(&data), sizeof(T));
}

struct ProcessData {
  std::string name;
  Pid pid;
};

struct Page {
  MemoryAddress address;
  size_t size;
};

}  // namespace maia
