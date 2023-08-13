
#include <regex>

#include <gtest/gtest.h>

#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scanner.h"
#include "maiascan/scanner/types.h"

namespace maia {

namespace {

std::optional<Pid> GetPidFromProcessName(const std::string &proc_name) {
  std::regex pattern{"^" + proc_name + ".*", std::regex_constants::icase};
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
  ASSERT_TRUE(pid);
  Process process{*pid};
  const auto &pages = process.QueryPages();
  int a = 2;
}

}  // namespace maia
