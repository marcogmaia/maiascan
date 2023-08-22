#pragma once

#include <vector>

#include <tl/expected.hpp>
#include <tl/optional.hpp>

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

  tl::optional<Bytes> ReadPage(const Page& page) const;

  tl::expected<void, std::string> ReadIntoBuffer(MemoryAddress address, BytesView buffer) const;

  tl::expected<void, std::string> Write(MemoryAddress address, BytesView value);

  tl::optional<Matches> Find(BytesView needle);

  Pid pid() const { return pid_; }

  template <CScannable T>
  tl::optional<T> Read(MemoryAddress address) {
    T buffer;
    auto buffer_view = BytesView(std::bit_cast<std::byte*>(std::addressof(buffer)), sizeof buffer);
    if (!ReadIntoBuffer(address, buffer_view)) {
      return tl::nullopt;
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
