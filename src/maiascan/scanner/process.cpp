
#include <afx.h>

#include "maiascan/scanner/process.h"

#include <bit>
#include <iostream>
#include <string>
#include <vector>

#include <Psapi.h>
#include <TlHelp32.h>
#include <fmt/core.h>
#include <expected>
#include <optional>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

namespace {

bool IsPageHackable(const MEMORY_BASIC_INFORMATION& page) {
  return page.State == MEM_COMMIT && page.Type == MEM_PRIVATE && page.Protect == PAGE_READWRITE;
}

std::optional<MEMORY_BASIC_INFORMATION> QueryPage(HANDLE handle, MemoryAddress address) {
  MEMORY_BASIC_INFORMATION page;
  if (VirtualQueryEx(handle, address, &page, sizeof page) != sizeof(page)) {
    return std::nullopt;
  };
  return page;
}

std::vector<Page> GetCheatablePages(HANDLE process_handle) {
  std::vector<Page> pages;
  pages.reserve(32);

  MemoryAddress address = nullptr;

  while (auto page = QueryPage(process_handle, address)) {
    if (IsPageHackable(*page)) {
      pages.emplace_back(address, page->RegionSize);
    }
    address = NextAddress(address, page->RegionSize);
  }

  pages.shrink_to_fit();
  return pages;
}

MemoryAddress GetBaseAddress(Pid pid) {
  auto* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return {};
  }
  MODULEENTRY32 mod_entry{.dwSize = sizeof(MODULEENTRY32)};
  bool success = Module32First(snapshot, &mod_entry) != 0;
  if (!success) {
    auto err = GetLastError();
    return {};
  }
  return std::bit_cast<MemoryAddress>(mod_entry.modBaseAddr);
}

void PrintAllProcessModules(Pid pid) {
  HANDLE hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

  if (hsnapshot == INVALID_HANDLE_VALUE) {
    return;
  }

  MODULEENTRY32 mod_entry = {0};
  mod_entry.dwSize = sizeof(MODULEENTRY32);
  for (bool ok = Module32First(hsnapshot, &mod_entry) != 0; ok; ok = Module32Next(hsnapshot, &mod_entry) != 0) {
    std::cout << fmt::format(
        "{:20} -- Addr: {}\n", mod_entry.szModule, std::bit_cast<MemoryAddress>(mod_entry.modBaseAddr));
  }

  CloseHandle(hsnapshot);
}

}  // namespace

const std::vector<Page>& Process::QueryPages() {
  pages_ = GetCheatablePages(handle_);
  return pages_;
}

std::optional<Bytes> Process::ReadPage(const Page& page) const {
  Bytes memory(page.size);

  size_t total{};
  if (!ReadProcessMemory(handle_, page.address, memory.data(), page.size, &total)) {
    return {};
  }
  memory.resize(total);
  return memory;
}

std::expected<void, std::string> Process::Write(MemoryAddress address, BytesView value) {
  if (!WriteProcessMemory(handle_, address, value.data(), value.size_bytes(), nullptr)) {
    return std::unexpected<std::string>{"Failed to write in memory"};
  }
  return {};
}

std::expected<void, std::string> Process::ReadIntoBuffer(MemoryAddress address, BytesView buffer) const {
  size_t size_read{};
  auto success = ReadProcessMemory(handle_, address, buffer.data(), buffer.size_bytes(), &size_read);
  if (!success || size_read != buffer.size_bytes()) {
    return std::unexpected("Failed to read address");
  }
  return {};
};

std::optional<Matches> Process::Find(BytesView needle) {
  return Search(*this, needle);
}

Process::Process(Pid pid)
    : pid_(pid), handle_(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid)), base_address_(GetBaseAddress(pid)) {
  PrintAllProcessModules(pid);
}

Process::~Process() {
  if (handle_) {
    CloseHandle(handle_);
  }
}

}  // namespace maia::scanner
