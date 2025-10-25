
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

template <typename T>
auto SearchT(Process& proc, T needle) {
  return Search(proc, ToBytesView(needle));
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
  int a = 2;
}

}  // namespace maia::scanner
