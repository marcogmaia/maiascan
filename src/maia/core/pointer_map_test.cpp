// Copyright (c) Maia

#include "maia/core/pointer_map.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "maia/tests/fake_process.h"

namespace maia::core {

namespace {

// Helper to set up a FakeProcess with some pointers
void SetupFakeProcess(test::FakeProcess& process) {
  // Base address is 0x100000, size 0x4000
  // Valid range: [0x100000, 0x104000)

  // 1. Valid pointer at offset 0 (0x100000) -> 0x100100
  process.WriteValue<uint64_t>(0, 0x100100);

  // 2. Valid pointer at offset 8 (0x100008) -> 0x100200
  process.WriteValue<uint64_t>(8, 0x100200);

  // 3. Invalid pointer at offset 16 (0x100010) -> 0x999999 (outside range)
  process.WriteValue<uint64_t>(16, 0x999999);

  // 4. Another valid pointer at offset 24 (0x100018) -> 0x100100 (duplicate
  // target)
  process.WriteValue<uint64_t>(24, 0x100100);
}

}  // namespace

class PointerMapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() / "maia_test";
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(temp_dir_);
  }

  std::filesystem::path temp_dir_;
};

TEST_F(PointerMapTest, GenerateFindsValidPointers) {
  test::FakeProcess process;
  SetupFakeProcess(process);

  auto map = PointerMap::Generate(process);
  ASSERT_TRUE(map.has_value());

  // We expect 3 valid pointers:
  // 0x100000 -> 0x100100
  // 0x100008 -> 0x100200
  // 0x100018 -> 0x100100
  EXPECT_EQ(map->GetEntryCount(), 3);
  EXPECT_EQ(map->GetPointerSize(), 4);  // 4 bytes because all addresses < 4GB
  EXPECT_EQ(map->GetProcessName(), "test_app.exe");
}

TEST_F(PointerMapTest, FindPointersToRange) {
  test::FakeProcess process;
  SetupFakeProcess(process);
  auto map = PointerMap::Generate(process);
  ASSERT_TRUE(map.has_value());

  // Search for pointers to 0x100100
  auto results = map->FindPointersToRange(0x100100, 0x100100);
  EXPECT_EQ(results.size(), 2);

  std::vector<uint64_t> addresses;
  for (const auto& entry : results) {
    EXPECT_EQ(entry.value, 0x100100);
    addresses.push_back(entry.address);
  }
  std::sort(addresses.begin(), addresses.end());
  EXPECT_EQ(addresses[0], 0x100000);
  EXPECT_EQ(addresses[1], 0x100018);

  // Search for pointers to [0x100200, 0x100300]
  results = map->FindPointersToRange(0x100200, 0x100300);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].address, 0x100008);
  EXPECT_EQ(results[0].value, 0x100200);

  // Search for non-existent target
  results = map->FindPointersToRange(0x500000, 0x500000);
  EXPECT_TRUE(results.empty());
}

TEST_F(PointerMapTest, SaveAndLoad) {
  test::FakeProcess process;
  SetupFakeProcess(process);
  auto map = PointerMap::Generate(process);
  ASSERT_TRUE(map.has_value());

  auto path = temp_dir_ / "test.pmap";
  ASSERT_TRUE(map->Save(path));

  auto loaded_map = PointerMap::Load(path);
  ASSERT_TRUE(loaded_map.has_value());

  EXPECT_EQ(loaded_map->GetEntryCount(), map->GetEntryCount());
  EXPECT_EQ(loaded_map->GetPointerSize(), map->GetPointerSize());
  EXPECT_EQ(loaded_map->GetProcessName(), map->GetProcessName());
  EXPECT_EQ(loaded_map->GetTimestamp(), map->GetTimestamp());

  // Verify content
  auto results = loaded_map->FindPointersToRange(0x100100, 0x100100);
  EXPECT_EQ(results.size(), 2);
}

}  // namespace maia::core
