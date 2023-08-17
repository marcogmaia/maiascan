#pragma once

#include <Windows.h>

#include <vector>

#include <tl/optional.hpp>

#include "maiascan/scanner/types.h"

namespace maia {

class Process {
 public:
  explicit Process(Pid pid) : handle_(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid)) {}

  ~Process() {
    if (handle_) {
      CloseHandle(handle_);
    }
  }

  const std::vector<MemoryPage>& QueryPages();

  tl::optional<Bytes> ReadPage(const MemoryPage& page) const;

 private:
  HANDLE handle_{};
  std::vector<MemoryPage> pages_;
};

}  // namespace maia
