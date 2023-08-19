
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace maia {

using MemoryAddress = uint8_t*;
using Pid = uint32_t;

using Bytes = std::vector<std::byte>;
using BytesView = std::span<const std::byte>;

struct ProcessData {
  std::string name;
  Pid pid;
};

// using MemoryPage = std::span<MemoryAddress>;
struct MemoryPage {
  MemoryAddress address;
  size_t size;
};

}  // namespace maia
