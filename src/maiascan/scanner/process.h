#pragma once

#include <Windows.h>

#include <vector>

#include <tl/expected.hpp>
#include <tl/optional.hpp>

#include "maiascan/scanner/match.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

class Process {
 public:
  explicit Process(Pid pid);

  ~Process() {
    if (handle_) {
      CloseHandle(handle_);
    }
  }

  const std::vector<Page>& QueryPages();

  tl::optional<Bytes> ReadPage(const Page& page) const;

  tl::expected<void, std::string> ReadIntoBuffer(MemoryAddress address, BytesView buffer) const;

  tl::expected<void, std::string> Write(MemoryAddress address, BytesView value);

  tl::optional<Matches> Find(BytesView needle);

  Pid pid() const { return pid_; }

 private:
  Pid pid_;
  HANDLE handle_{};
  MemoryAddress base_address_{};
  std::vector<Page> pages_;
};

}  // namespace maia::scanner
