
#include <afx.h>

#include "maiascan/scanner/process.h"

#include <bit>
#include <string>
#include <vector>

#include <Psapi.h>
#include <TlHelp32.h>
#include <tl/expected.hpp>
#include <tl/optional.hpp>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

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

tl::optional<MemoryAddress> GetProcessBaseAddress(Pid pid) {
  auto* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
  MODULEENTRY32 mod_entry{.dwSize = sizeof(MODULEENTRY32)};
  bool success = Module32First(snapshot, &mod_entry) != 0;
  if (!success) {
    auto err = GetLastError();
    return tl::nullopt;
  }
  return std::bit_cast<MemoryAddress>(mod_entry.modBaseAddr);
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

tl::optional<MemoryAddress> Process::GetBaseAddress() {
  std::string name(1024, 0);
  K32GetProcessImageFileNameA(handle_, name.data(), name.size());
  auto* mod_handle = GetModuleHandleA(name.data());

  MODULEINFO module_info{};
  K32GetModuleInformation(handle_, mod_handle, &module_info, sizeof module_info);
  // GetProcessInformation(HANDLE hProcess, PROCESS_INFORMATION_CLASS ProcessInformationClass, LPVOID
  // ProcessInformation, DWORD ProcessInformationSize)

  // auto aaa = GetBaseAddress2(handle_);
  auto addrr = GetProcessBaseAddress(pid_);
  return addrr.value_or(nullptr);
}

tl::expected<void, std::string> Process::Write(MemoryAddress address, BytesView value) {
  if (!WriteProcessMemory(handle_, address, value.data(), value.size_bytes(), nullptr)) {
    return tl::unexpected<std::string>{"Failed to write in memory"};
  }
  return {};
}

tl::expected<void, std::string> Process::ReadIntoBuffer(MemoryAddress address, BytesView buffer) const {
  size_t size_read{};
  auto success = ReadProcessMemory(handle_, address, buffer.data(), buffer.size_bytes(), &size_read);
  if (!success || size_read != buffer.size_bytes()) {
    return tl::unexpected("Failed to read address");
  }
  return {};
};

tl::optional<Matches> Process::Find(BytesView needle) {
  return Search(*this, needle);
}

}  // namespace maia::scanner
