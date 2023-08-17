
#include <afx.h>

#include "maiascan/scanner/process.h"

#include <vector>

#include <Psapi.h>
#include <TlHelp32.h>
#include <tl/optional.hpp>

#include "maiascan/scanner/types.h"

namespace maia {

namespace {

bool CanCheatPage(const MEMORY_BASIC_INFORMATION& page) {
  return page.State == MEM_COMMIT && page.Type == MEM_PRIVATE && page.Protect == PAGE_READWRITE;
}

tl::optional<MEMORY_BASIC_INFORMATION> QueryPage(HANDLE handle, MemoryAddress address) {
  MEMORY_BASIC_INFORMATION page;
  if (VirtualQueryEx(handle, address, &page, sizeof(page)) != sizeof(page)) {
    return tl::nullopt;
  };
  return page;
}

std::vector<MemoryPage> GetCheatablePages(HANDLE process_handle) {
  std::vector<MemoryPage> pages;
  pages.reserve(32);

  MemoryAddress address{};

  while (auto page = QueryPage(process_handle, address)) {
    if (CanCheatPage(*page)) {
      pages.emplace_back(address, page->RegionSize);
    }
    std::advance(address, page->RegionSize);
  }

  return pages;
}

}  // namespace

const std::vector<MemoryPage>& Process::QueryPages() {
  pages_ = GetCheatablePages(handle_);
  return pages_;
}

// memory_t WindowsProcess::read(MemoryPage page) const {
//   memory_t memory(page.size);

//   DWORD total;
//   if (!ReadProcessMemory(handle, page.address, memory.data(),
//                          page.size, &total)) {
//     std::cerr << "Failed to read process memory." << std::endl;
//     exit(1);
//   }

//   memory.resize(total);
//   return memory;
// }

tl::optional<Bytes> Process::ReadPage(const MemoryPage& page) const {
  Bytes memory(page.size);

  size_t total{};
  if (!ReadProcessMemory(handle_, page.address, memory.data(), page.size, &total)) {
    return {};
  }
  memory.resize(total);
  return memory;
}

}  // namespace maia
