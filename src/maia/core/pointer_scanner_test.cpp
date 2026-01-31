// Copyright (c) Maia

#include "maia/core/pointer_scanner.h"

#include <gtest/gtest.h>

#include "maia/core/pointer_map.h"
#include "maia/tests/fake_process.h"

namespace maia::core {

namespace {

// Scenario:
// game.exe base: 0x400000, size 0x10000.
// Heap base: 0x100000, size 0x1000.
//
// Target Address: 0x100080 (Health)
//
// Path: game.exe+0x100 -> 0 -> 0x20
//   [game.exe + 0x100] (0x400100) points to 0x100020
//   [0x100020] points to 0x100060
//   0x100060 + 0x20 = 0x100080 (Target)

template <typename T>
void WriteAt(test::FakeProcess& process, uintptr_t address, T value) {
  process.WriteMemory(address, std::as_bytes(std::span<const T, 1>{&value, 1}));
}

void SetupChain(test::FakeProcess& process) {
  // Define module
  process.AddModule("game.exe", 0x400000, 0x10000);

  // Target value (just so memory is valid)
  WriteAt<uint32_t>(process, 0x100080, 999);

  // P1 at 0x100020 points to 0x100060
  WriteAt<uint64_t>(process, 0x100020, 0x100060);

  // P2 (Static) at 0x400100 points to 0x100020
  WriteAt<uint64_t>(process, 0x400100, 0x100020);
}

}  // namespace

class PointerScannerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a large FakeProcess to support 0x400000 address range.
    process_ = std::make_unique<test::FakeProcess>(0x500000);

    // Set base address to 0 for absolute addressing simulation.
  }

  std::unique_ptr<test::FakeProcess> process_;
};

TEST_F(PointerScannerTest, FindSimplePath) {
  SetupChain(*process_);

  // Generate map
  auto map = PointerMap::Generate(*process_);
  ASSERT_TRUE(map.has_value());

  // Verify map content briefly
  auto results = map->FindPointersToRange(0x100020, 0x100020);
  bool found_p2 = false;
  for (const auto& entry : results) {
    if (entry.address == 0x400100) {
      found_p2 = true;
    }
  }
  EXPECT_TRUE(found_p2);

  // Scan
  PointerScanner scanner;
  PointerScanConfig config;
  config.target_address = 0x100080;
  config.max_level = 2;
  config.max_offset = 0x100;

  auto result = scanner.FindPaths(*map, config, process_->GetModules());
  ASSERT_TRUE(result.success);

  // We found 2 valid paths:
  // 1. game.exe+100 -> 0x60 (Direct offset from P2 to Target)
  // 2. game.exe+100 -> 0 -> 0x20 (P2 -> P1 -> Target)
  ASSERT_GE(result.paths.size(), 2);

  // Find the deep path
  bool found_deep_path = false;
  for (const auto& p : result.paths) {
    if (p.offsets.size() == 2 && p.offsets[1] == 0x20) {
      found_deep_path = true;
      EXPECT_EQ(p.module_name, "game.exe");
      EXPECT_EQ(p.module_offset, 0x100);
      EXPECT_EQ(p.offsets[0], 0);

      // Test Resolution
      auto resolved = scanner.ResolvePath(*process_, p);
      ASSERT_TRUE(resolved.has_value());
      EXPECT_EQ(*resolved, 0x100080);
    }
  }
  EXPECT_TRUE(found_deep_path);
}

TEST_F(PointerScannerTest, FilterPaths) {
  SetupChain(*process_);

  // Create a dummy path that points to wrong location
  PointerPath bad_path;
  bad_path.base_address = 0x400100;
  bad_path.offsets = {0, 0x99};  // Wrong offset

  PointerPath good_path;
  good_path.base_address = 0x400100;
  good_path.offsets = {0, 0x20};

  std::vector<PointerPath> paths = {bad_path, good_path};
  PointerScanner scanner;

  auto valid = scanner.FilterPaths(*process_, paths, 0x100080);
  ASSERT_EQ(valid.size(), 1);
  EXPECT_EQ(valid[0].offsets[1], 0x20);
}

}  // namespace maia::core
