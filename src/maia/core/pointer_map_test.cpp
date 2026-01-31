// Copyright (c) Maia

#include "maia/core/pointer_map.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
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
  void SetUp() override {}

  void TearDown() override {}
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

TEST_F(PointerMapTest, SaveAndLoadStream) {
  test::FakeProcess process;
  SetupFakeProcess(process);
  auto map = PointerMap::Generate(process);
  ASSERT_TRUE(map.has_value());

  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  ASSERT_TRUE(map->Save(ss));

  // Reset stream for reading
  ss.seekg(0);

  auto loaded_map = PointerMap::Load(ss);
  ASSERT_TRUE(loaded_map.has_value());

  EXPECT_EQ(loaded_map->GetEntryCount(), map->GetEntryCount());
  EXPECT_EQ(loaded_map->GetPointerSize(), map->GetPointerSize());
  EXPECT_EQ(loaded_map->GetProcessName(), map->GetProcessName());
  EXPECT_EQ(loaded_map->GetTimestamp(), map->GetTimestamp());

  // Verify content
  auto results = loaded_map->FindPointersToRange(0x100100, 0x100100);
  EXPECT_EQ(results.size(), 2);
}

TEST_F(PointerMapTest, LoadRejectsMalformedStream) {
  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
  char bad_magic[8] = {'F', 'A', 'K', 'E', 'F', 'I', 'L', 'E'};
  ss.write(bad_magic, 8);
  ss.seekg(0);

  auto loaded = PointerMap::Load(ss);
  EXPECT_FALSE(loaded.has_value());
}

TEST_F(PointerMapTest, LoadRejectsStreamWithHugeEntryCount) {
  std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

  // Write valid header with huge entry_count
  char magic[8] = {'M', 'A', 'I', 'A', 'P', 'T', 'R', '\0'};
  ss.write(magic, 8);

  uint32_t version = 1;
  ss.write(reinterpret_cast<const char*>(&version), 4);

  uint32_t pointer_size = 8;
  ss.write(reinterpret_cast<const char*>(&pointer_size), 4);

  uint64_t entry_count = 1000000000;  // 1 billion entries
  ss.write(reinterpret_cast<const char*>(&entry_count), 8);

  // Write minimal other header fields
  uint64_t timestamp = 0;
  ss.write(reinterpret_cast<const char*>(&timestamp), 8);

  uint32_t flags = 0;
  ss.write(reinterpret_cast<const char*>(&flags), 4);

  uint32_t name_len = 0;
  ss.write(reinterpret_cast<const char*>(&name_len), 4);

  // Write padding
  uint8_t padding[24] = {0};
  ss.write(reinterpret_cast<const char*>(padding), 24);

  ss.seekg(0);

  // Loading should fail gracefully, not crash or allocate huge memory
  auto loaded = PointerMap::Load(ss);
  EXPECT_FALSE(loaded.has_value());
}

TEST_F(PointerMapTest, PointerAtRegionBaseIsValid) {
  // Regression test: pointers that exactly match a memory region's
  // base address should be considered valid.
  // Bug: IsValidPointer uses std::lower_bound which incorrectly rejects
  // pointers at the exact start of the first region.

  test::FakeProcess process;

  // The FakeProcess has a memory region at [0x100000, 0x104000)
  // Write a pointer at offset 0 that points exactly to the region base:
  // 0x100000
  process.WriteValue<uint64_t>(0, 0x100000);

  // Write another pointer at offset 8 that points just past the base
  process.WriteValue<uint64_t>(8, 0x100001);

  auto map = PointerMap::Generate(process);
  ASSERT_TRUE(map.has_value());

  // Both pointers should be found as valid.
  // The bug causes the pointer to 0x100000 to be incorrectly rejected.
  EXPECT_EQ(map->GetEntryCount(), 2)
      << "Pointer to exact region base (0x100000) was incorrectly rejected";

  // Verify we can find the pointer to the region base
  auto results = map->FindPointersToRange(0x100000, 0x100000);
  EXPECT_EQ(results.size(), 1)
      << "Should find exactly one pointer to region base address";
  EXPECT_EQ(results[0].address, 0x100000)
      << "Pointer at region base should point to region base";
}

}  // namespace maia::core
