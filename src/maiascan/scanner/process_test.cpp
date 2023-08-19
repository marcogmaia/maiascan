
#include <algorithm>
#include <bit>
#include <numeric>
#include <regex>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scanner.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

namespace {

std::optional<Pid> GetPidFromProcessName(const std::string &proc_name) {
  std::regex pattern{fmt::format("^{}.*", proc_name), std::regex_constants::icase};
  std::smatch match{};
  auto procs = GetProcs();
  for (const auto &proc : procs) {
    if (std::regex_match(proc.name, match, pattern)) {
      return proc.pid;
    }
  }
  return std::nullopt;
}

template <typename T>
BytesView ToBytesView(T data) {
  constexpr bool kIsCString = std::is_same_v<char *, std::decay_t<T>> || std::is_same_v<const char *, std::decay_t<T>>;
  if constexpr (kIsCString) {
    return BytesView(std::bit_cast<std::byte *>(data), strlen(data));
  }
  return BytesView(std::bit_cast<std::byte *>(&data), sizeof(T));
}

template <typename T>
auto SearchT(Process &proc, T needle) {
  return Search(proc, ToBytesView(needle));
}

// template <>
BytesView ToBytesView(std::string &data) {
  return BytesView(std::bit_cast<std::byte *>(data.data()), data.size());
}


}  // namespace

TEST(Process, AttachScan) {
  auto pid = GetPidFromProcessName("fakegame");
  ASSERT_TRUE(pid) << "Make sure that the `fakegame` is running.";
  Process process{*pid};
  int32_t needle = 1337;
  auto matches = SearchT(process, needle);
  ASSERT_TRUE(matches);
  auto vals = *matches;
  auto *base_addr = process.GetBaseAddress().value_or(nullptr);
  int a = 2;
}

TEST(Process, NarrowValue) {
  auto pid = GetPidFromProcessName("fakegame");
  ASSERT_TRUE(pid) << "Make sure that the `fakegame` is running.";
  Process process{*pid};
  auto matches = SearchT(process, "hello world");
  ASSERT_TRUE(matches);

  std::string buffer(7, 0);

  auto addresses = GetAddressMatches(*matches);
  ASSERT_TRUE(!addresses.empty());
  ASSERT_TRUE(process.ReadIntoBuffer(addresses.front(), ToBytesView(buffer)));

  auto *base_addr = process.GetBaseAddress().value_or(nullptr);
  int a = 2;
}

}  // namespace maia::scanner
