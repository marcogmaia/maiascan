
#include <ranges>

#include <gtest/gtest.h>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/scan.h"

namespace maia::scanner {

TEST(Scan, Test) {
  auto pid = GetPidFromProcessName("fakegame");
  ASSERT_TRUE(pid) << "Make sure fakegame is running.";
  auto process = std::make_shared<Process>(*pid);
  auto scan = Scan{process};

  int needle = 1337;
  const auto &scan_result = scan.Find(needle);

  ASSERT_TRUE(!scan_result.empty());

  int new_needle = 1340;
  scan.Find(new_needle);
  scan.FilterChanged();
  EXPECT_EQ(scan.scan().size(), 1);
  EXPECT_TRUE(std::ranges::equal(scan.scan().front().bytes, ToBytesView(new_needle)));
}

TEST(Scan, MemoryAddress) {
  auto pid = GetPidFromProcessName("fakegame");
  ASSERT_TRUE(pid) << "Make sure fakegame is running.";
  auto process = std::make_shared<Process>(*pid);
  auto scan = Scan{process};

  int needle = 1337;
  const auto &scan_result = scan.Find(needle);

  ASSERT_TRUE(!scan.scan().empty());

  bool found = false;

  auto scan_addresses = scan.scan();
  MemoryAddress needle_address{};
  for (auto &scan_result : scan_addresses) {
    needle_address = scan_result.address;
    ASSERT_EQ(needle, *process->Read<int>(needle_address));
    found = !scan.Find(needle_address).empty();
    if (found) {
      break;
    }
  }

  for (auto &s : scan.scan()) {
    auto *fneedle = *process->Read<MemoryAddress>(s.address);
    EXPECT_EQ(needle_address, fneedle);
  }

  ASSERT_TRUE(found);
  ASSERT_TRUE(!scan_addresses.empty());

  for (auto &scan_result : scan_addresses) {
    EXPECT_EQ(needle, *std::bit_cast<decltype(needle) *>(scan_result.bytes.data()));
  }
}

}  // namespace maia::scanner
