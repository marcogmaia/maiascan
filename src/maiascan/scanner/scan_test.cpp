
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
  auto scan_result = scan.Find(needle);

  int buf{};
  auto view = BytesView(std::bit_cast<std::byte*>(&buf), sizeof buf);
  process.ReadIntoBuffer(std::bit_cast<MemoryAddress>(0x1e2402a58f0), view);

  ASSERT_TRUE(!scan_result.empty());
  ASSERT_TRUE(std::ranges::equal(scan_result.front().bytes, view));
  SUCCEED();
}

}  // namespace maia::scanner
