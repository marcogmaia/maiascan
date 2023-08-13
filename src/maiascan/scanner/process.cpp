
#include <afx.h>

#include "maiascan/scanner/process.h"

#include <optional>
#include <vector>

#include "maiascan/scanner/types.h"

namespace maia {

namespace {

bool CanCheatPage(const MEMORY_BASIC_INFORMATION& page) {
  return page.State == MEM_COMMIT && page.Type == MEM_PRIVATE && page.Protect == PAGE_READWRITE;
}

std::optional<MEMORY_BASIC_INFORMATION> QueryPage(HANDLE handle, MemoryAddress address) {
  MEMORY_BASIC_INFORMATION page;
  if (VirtualQueryEx(handle, address, &page, sizeof(page)) != sizeof(page)) {
    return std::nullopt;
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

}  // namespace maia
