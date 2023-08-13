
#pragma once

#include <cstdint>
#include <string>

namespace maia {

using MemoryAddress = uint8_t*;
using Pid = uint32_t;

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
