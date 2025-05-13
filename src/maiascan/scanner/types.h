
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
BytesView ToBytesView(T &data) {
  return BytesView(std::bit_cast<std::byte *>(&data), sizeof(T));
}

template <CFundamentalType T>
T BytesToFundametalType(BytesViewReadOnly view) {
  const auto *ptr = std::bit_cast<T *>(view.data());
  return *ptr;
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
