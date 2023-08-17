
#include <regex>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scanner.h"
#include "maiascan/scanner/types.h"

namespace maia {

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

}  // namespace

TEST(Process, AttachScan) {
  auto pid = GetPidFromProcessName("fakegame");
  ASSERT_TRUE(pid) << "Make sure that the `fakegame` is running.";
  Process process{*pid};
  const auto &pages = process.QueryPages();

  int32_t needle = 1337;
  auto *n = std::bit_cast<uint8_t *>(&needle);
  Bytes bbb(n, n + 4);
  auto matches = maia::Search(process, bbb);
  ASSERT_TRUE(matches);

  auto vals = *matches;

  int a = 2;
}

}  // namespace maia
