// Copyright (c) Maia

#include <Windows.h>

#include <Psapi.h>
#include <TlHelp32.h>

#include "maia/scanner/process.h"

#include <string>
#include <vector>

#include <fmt/core.h>
#include <expected>
#include <optional>

#include "maia/logging.h"
#include "maia/scanner/memory_common.h"

// #include "maia/scanner/engine.h"

namespace maia::scanner {

template <typename T>
concept CScannable = CFundamentalType<T> || std::is_pointer_v<T>;

// The `Process` is responsible to query data from the process, we can read the
// memory pages and retrieve the memory.
class Process {
 public:
  using ProcessHandle = MemoryPtr;

  explicit Process(Pid pid);

  ~Process();

  const std::vector<Page>& QueryPages();

  std::optional<std::vector<Byte>> ReadPage(const Page& page) const;

  // std::expected<void, std::string> ReadIntoBuffer(MemoryPtr address,
  //                                                 std::span<Byte> buffer)
  //                                                 const;

  std::expected<void, std::string> Write(MemoryPtr address,
                                         std::span<Byte> value);

  // TODO: Remove this from here. This is the job of the `Scan` class.
  // std::optional<Matches> Find(std::span<Byte> needle);

  Pid pid() const {
    return pid_;
  }

  template <CScannable T>
  std::optional<T> Read(MemoryPtr address) {
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
  MemoryPtr base_address_{};
  std::vector<Page> pages_;
};

namespace {

MemoryPtr NextAddress(MemoryPtr address, int64_t diff = 1) {
  return static_cast<MemoryPtr>(
      std::next(static_cast<uint8_t*>(address), diff));
};

bool IsPageModifiable(const MEMORY_BASIC_INFORMATION& page) {
  return page.State == MEM_COMMIT && page.Type == MEM_PRIVATE &&
         page.Protect == PAGE_READWRITE;
}

std::optional<MEMORY_BASIC_INFORMATION> QueryPage(HANDLE handle,
                                                  MemoryPtr address) {
  MEMORY_BASIC_INFORMATION page;
  if (VirtualQueryEx(handle, address, &page, sizeof page) != sizeof(page)) {
    return std::nullopt;
  };
  return page;
}

std::vector<Page> GetModifiablePages(HANDLE process_handle) {
  std::vector<Page> pages;
  pages.reserve(32);

  MemoryPtr address = nullptr;

  while (auto page = QueryPage(process_handle, address)) {
    if (IsPageModifiable(*page)) {
      pages.emplace_back(address, page->RegionSize);
    }
    address = NextAddress(address, page->RegionSize);
  }

  pages.shrink_to_fit();
  return pages;
}

MemoryPtr GetBaseAddress(Pid pid) {
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
  return mod_entry.modBaseAddr;
}

void PrintAllProcessModules(Pid pid) {
  HANDLE h_snapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

  if (h_snapshot == INVALID_HANDLE_VALUE) {
    return;
  }

  MODULEENTRY32 mod_entry = {0};
  mod_entry.dwSize = sizeof(MODULEENTRY32);
  for (bool ok = Module32First(h_snapshot, &mod_entry) != 0; ok;
       ok = Module32Next(h_snapshot, &mod_entry) != 0) {
    LogInfo("{:20} -- Addr: {:p}",
            mod_entry.szModule,
            fmt::ptr(mod_entry.modBaseAddr));
  }

  CloseHandle(h_snapshot);
}

}  // namespace

const std::vector<Page>& Process::QueryPages() {
  pages_ = GetModifiablePages(handle_);
  return pages_;
}

std::optional<std::vector<Byte>> Process::ReadPage(const Page& page) const {
  std::vector<Byte> memory(page.size);
  size_t total{};
  if (!ReadProcessMemory(
          handle_, page.address, memory.data(), page.size, &total)) {
    return {};
  }
  memory.resize(total);
  return memory;
}

std::expected<void, std::string> Process::Write(MemoryPtr address,
                                                std::span<Byte> value) {
  if (!WriteProcessMemory(
          handle_, address, value.data(), value.size_bytes(), nullptr)) {
    return std::unexpected<std::string>{"Failed to write in memory"};
  }
  return {};
}

// std::expected<void, std::string> Process::ReadIntoBuffer(
//     MemoryPtr address, std::span<Byte> buffer) const {
//   size_t size_read{};
//   auto success = ReadProcessMemory(
//       handle_, address, buffer.data(), buffer.size_bytes(), &size_read);
//   if (!success || size_read != buffer.size_bytes()) {
//     return std::unexpected("Failed to read address");
//   }
//   return {};
// };

// std::optional<Matches> Process::Find(std::span<Byte> needle) {
//   return Search(*this, needle);
// }

Process::Process(Pid pid)
    : pid_(pid),
      handle_(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid)),
      base_address_(GetBaseAddress(pid)) {
  PrintAllProcessModules(pid);
}

Process::~Process() {
  if (handle_) {
    CloseHandle(handle_);
  }
}

std::expected<void, std::string> ReadIntoBuffer2(MemoryPtr handle,
                                                 MemoryPtr address,
                                                 std::span<std::byte> buffer) {
  size_t size_read{};
  auto success = ReadProcessMemory(
      handle, address, buffer.data(), buffer.size_bytes(), &size_read);
  if (!success || size_read != buffer.size_bytes()) {
    return std::unexpected("Failed to read address");
  }
  return {};
};

std::vector<MemoryRegion> LiveProcessAccessor::GetMemoryRegions() {
  return {};
}

bool LiveProcessAccessor::WriteMemory(MemoryPtr address,
                                      std::span<const std::byte> data) {
  // TODO: Implement this pure virtual method.
  // static_assert(false, "Method `WriteMemory` is not implemented.");
  return false;
}

bool LiveProcessAccessor::ReadMemory(MemoryPtr address,
                                     std::span<std::byte> buffer) {
  return static_cast<bool>(ReadIntoBuffer2(handle_, address, buffer));
}

}  // namespace maia::scanner
