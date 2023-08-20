
#include <ranges>

#include <gtest/gtest.h>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/scan.h"

namespace maia::scanner {

TEST(Scan, Test) {
  auto pid = GetPidFromProcessName("fakegame");
  ASSERT_TRUE(pid) << "Make sure fakegame is running.";
  auto process = Process{*pid};
  auto scan = Scan{&process};

  int needle = 1337;
  const auto &scan_result = scan.Find(needle);

  ASSERT_TRUE(!scan_result.empty());

  int new_needle = 1340;
  scan.Find(new_needle);
  scan.FilterChanged();
  EXPECT_EQ(scan.scan().size(), 1);
  EXPECT_TRUE(std::ranges::equal(scan.scan().front().bytes, ToBytesView(new_needle)));
}

}  // namespace maia::scanner
