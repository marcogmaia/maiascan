#pragma once

#include <vector>

#include <expected>
#include <optional>

#include "maiascan/scanner/match.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

template <typename T>
concept CScannable = CFundamentalType<T> || std::is_pointer_v<T>;

class Process {
 public:
  using ProcessHandle = MemoryAddress;

  explicit Process(Pid pid);

  ~Process();

  const std::vector<Page>& QueryPages();

  std::optional<std::vector<Byte>> ReadPage(const Page& page) const;

  std::expected<void, std::string> ReadIntoBuffer(MemoryAddress address,
                                                  std::span<Byte> buffer) const;

  std::expected<void, std::string> Write(MemoryAddress address,
                                         std::span<Byte> value);

  std::optional<Matches> Find(std::span<Byte> needle);

  Pid pid() const {
    return pid_;
  }

  template <CScannable T>
  std::optional<T> Read(MemoryAddress address) {
    T buffer;
    std::span<Byte> buffer_view = std::span<Byte>(
        std::bit_cast<Byte*>(std::addressof(buffer)), sizeof(buffer));
    if (!ReadIntoBuffer(address, buffer_view)) {
      return std::nullopt;
    }
    return buffer;
  }

 private:
  Pid pid_{};
  ProcessHandle handle_{};
  MemoryAddress base_address_{};
  std::vector<Page> pages_;
};

}  // namespace maia::scanner
