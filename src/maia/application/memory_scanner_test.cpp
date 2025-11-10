// Copyright (c) Maia

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "maia/application/memory_scanner.h"
#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"

namespace maia {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;

class MockProcess : public IProcess {
 public:
  MOCK_METHOD(bool,
              ReadMemory,
              (uintptr_t, std::span<std::byte>),
              (const, override));
  MOCK_METHOD(bool,
              WriteMemory,
              (uintptr_t, std::span<const std::byte>),
              (override));
  MOCK_METHOD(std::vector<MemoryRegion>,
              GetMemoryRegions,
              (),
              (const, override));
  MOCK_METHOD(uint32_t, GetProcessId, (), (const, override));
  MOCK_METHOD(std::string, GetProcessName, (), (const, override));
  MOCK_METHOD(bool, IsProcessValid, (), (const, override));
  MOCK_METHOD(uintptr_t, GetBaseAddress, (), (const, override));
};

class MemoryScannerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup memory regions.
    regions_ = {
        MemoryRegion{.base_address = 0x1000,
                     .size = 0x1000,
                     // 4KB region
                     .protection_flags = 0x04},
        MemoryRegion{.base_address = 0x2000,
                     .size = 0x1000,
                     // Another 4KB region
                     .protection_flags = 0x04}
    };

    // Setup mock process
    EXPECT_CALL(process_, GetMemoryRegions()).WillRepeatedly(Return(regions_));
    EXPECT_CALL(process_, IsProcessValid()).WillRepeatedly(Return(true));
  }

  MockProcess process_;
  std::vector<MemoryRegion> regions_;
};

TEST_F(MemoryScannerTest, NewScanExactValueU32) {
  MemoryScanner scanner(process_);

  // Setup memory with test values
  std::vector<std::byte> memory_data(0x1000);
  uint32_t* ints = reinterpret_cast<uint32_t*>(memory_data.data());
  ints[0] = 42;   // at 0x1000
  ints[1] = 100;  // at 0x1004
  ints[2] = 42;   // at 0x1008 (another match)
  ints[3] = 200;  // at 0x100C

  // Mock all ReadMemory calls
  EXPECT_CALL(process_, ReadMemory(testing::_, testing::_))
      .WillRepeatedly(
          [&memory_data, &ints](uintptr_t addr, std::span<std::byte> buffer) {
            // Region read (large buffer)
            if (addr == 0x1000 && buffer.size() == 0x1000) {
              std::memcpy(buffer.data(), memory_data.data(), buffer.size());
              return true;
            }
            // Region read for second region
            if (addr == 0x2000 && buffer.size() == 0x1000) {
              std::fill(buffer.begin(), buffer.end(), std::byte{0});
              return true;
            }
            // Individual address reads for UpdateSnapshotValues
            if (buffer.size() == sizeof(uint32_t)) {
              if (addr == 0x1000) {
                std::memcpy(buffer.data(), &ints[0], sizeof(uint32_t));
                return true;
              } else if (addr == 0x1008) {
                std::memcpy(buffer.data(), &ints[2], sizeof(uint32_t));
                return true;
              }
            }
            // Default: fill with zeros
            std::fill(buffer.begin(), buffer.end(), std::byte{0});
            return true;
          });

  auto params = MakeScanParams<uint32_t>(ScanComparison::kExactValue, 42);
  auto result = scanner.NewScan(params);

  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result.addresses(), ElementsAre(0x1000, 0x1008));

  std::vector<uint32_t> values;
  std::ranges::copy(result.values<uint32_t>(), std::back_inserter(values));
  EXPECT_THAT(values, ElementsAre(42, 42));
}

TEST_F(MemoryScannerTest, NewScanGreaterThanI32) {
  MemoryScanner scanner(process_);

  // Setup memory with test values
  std::vector<std::byte> memory_data(0x1000);
  int32_t* ints = reinterpret_cast<int32_t*>(memory_data.data());
  ints[0] = 10;  // at 0x1000
  ints[1] = 25;  // at 0x1004
  ints[2] = 5;   // at 0x1008
  ints[3] = 30;  // at 0x100C

  // Mock all ReadMemory calls with a single expectation
  EXPECT_CALL(process_, ReadMemory(testing::_, testing::_))
      .WillRepeatedly(
          [&memory_data, &ints](uintptr_t addr, std::span<std::byte> buffer) {
            // Region read (large buffer)
            if (addr == 0x1000 && buffer.size() == 0x1000) {
              std::memcpy(buffer.data(), memory_data.data(), buffer.size());
              return true;
            }
            // Region read for second region
            if (addr == 0x2000 && buffer.size() == 0x1000) {
              std::fill(buffer.begin(), buffer.end(), std::byte{0});
              return true;
            }
            // Individual address reads for UpdateSnapshotValues
            if (buffer.size() == sizeof(int32_t)) {
              if (addr == 0x1004) {
                std::memcpy(buffer.data(), &ints[1], sizeof(int32_t));
                return true;
              } else if (addr == 0x100C) {
                std::memcpy(buffer.data(), &ints[3], sizeof(int32_t));
                return true;
              }
            }
            // Default: fill with zeros
            std::fill(buffer.begin(), buffer.end(), std::byte{0});
            return true;
          });

  auto params = MakeScanParams<int32_t>(ScanComparison::kGreaterThan, 20);
  auto result = scanner.NewScan(params);

  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result.addresses(), ElementsAre(0x1004, 0x100C));

  std::vector<int32_t> values;
  std::ranges::copy(result.values<int32_t>(), std::back_inserter(values));
  EXPECT_THAT(values, ElementsAre(25, 30));
}

TEST_F(MemoryScannerTest, NewScanLessThanFloat) {
  MemoryScanner scanner(process_);

  // Setup memory with test values for both regions
  std::vector<std::byte> memory_data1(0x1000);
  std::vector<std::byte> memory_data2(0x1000);
  float* floats1 = reinterpret_cast<float*>(memory_data1.data());
  float* floats2 = reinterpret_cast<float*>(memory_data2.data());

  // Initialize both regions with values > 2.0f (10.0f)
  for (size_t i = 0; i < memory_data1.size() / sizeof(float); ++i) {
    floats1[i] = 10.0f;  // > 2.0f, should not match
    floats2[i] = 10.0f;  // > 2.0f, should not match
  }

  // Set test values in first region that should match
  floats1[0] = 1.5f;  // at 0x1000 - should match
  floats1[2] = 1.0f;  // at 0x1008 - should match

  // No matches in second region (all 10.0f)

  // Mock all ReadMemory calls - SIMPLIFIED: Just return the data from our
  // vectors
  EXPECT_CALL(process_, ReadMemory(testing::_, testing::_))
      .WillRepeatedly([&memory_data1, &memory_data2](
                          uintptr_t addr, std::span<std::byte> buffer) {
        // Region 1 read (large buffer)
        if (addr == 0x1000 && buffer.size() == 0x1000) {
          std::memcpy(buffer.data(), memory_data1.data(), buffer.size());
          return true;
        }
        // Region 2 read (large buffer)
        if (addr == 0x2000 && buffer.size() == 0x1000) {
          std::memcpy(buffer.data(), memory_data2.data(), buffer.size());
          return true;
        }
        // Individual address reads for UpdateSnapshotValues
        // Just calculate the offset and copy from the appropriate vector
        if (buffer.size() == sizeof(float)) {
          if (addr >= 0x1000 && addr < 0x2000) {
            size_t offset = addr - 0x1000;
            if (offset + sizeof(float) <= memory_data1.size()) {
              std::memcpy(buffer.data(), &memory_data1[offset], sizeof(float));
              return true;
            }
          } else if (addr >= 0x2000 && addr < 0x3000) {
            size_t offset = addr - 0x2000;
            if (offset + sizeof(float) <= memory_data2.size()) {
              std::memcpy(buffer.data(), &memory_data2[offset], sizeof(float));
              return true;
            }
          }
        }
        // Should never reach here if our math is correct
        return false;
      });

  auto params = MakeScanParams<float>(ScanComparison::kLessThan, 2.0f);
  auto result = scanner.NewScan(params);

  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result.addresses(), ElementsAre(0x1000, 0x1008));

  std::vector<float> values;
  std::ranges::copy(result.values<float>(), std::back_inserter(values));
  // Values are returned in address order: 0x1000 (1.5f) then 0x1008 (1.0f)
  EXPECT_THAT(values, ElementsAre(1.5f, 1.0f));
}

TEST_F(MemoryScannerTest, NewScanNotEqualU64) {
  MemoryScanner scanner(process_);

  // Setup memory with test values for both regions
  std::vector<std::byte> memory_data1(0x1000);
  std::vector<std::byte> memory_data2(0x1000);
  uint64_t* uints1 = reinterpret_cast<uint64_t*>(memory_data1.data());
  uint64_t* uints2 = reinterpret_cast<uint64_t*>(memory_data2.data());

  // Initialize both regions with values == 100 (so they won't match kNotEqual)
  for (size_t i = 0; i < memory_data1.size() / sizeof(uint64_t); ++i) {
    uints1[i] = 100;  // == 100, should not match kNotEqual
    uints2[i] = 100;  // == 100, should not match kNotEqual
  }

  // Set test values in first region that should match (!= 100)
  uints1[1] = 200;  // at 0x1008 - should match
  uints1[3] = 300;  // at 0x1018 - should match

  // No matches in second region (all 100)

  // Mock all ReadMemory calls - SIMPLIFIED: Just return the data from our
  // vectors
  EXPECT_CALL(process_, ReadMemory(testing::_, testing::_))
      .WillRepeatedly([&memory_data1, &memory_data2](
                          uintptr_t addr, std::span<std::byte> buffer) {
        // Region 1 read (large buffer)
        if (addr == 0x1000 && buffer.size() == 0x1000) {
          std::memcpy(buffer.data(), memory_data1.data(), buffer.size());
          return true;
        }
        // Region 2 read (large buffer)
        if (addr == 0x2000 && buffer.size() == 0x1000) {
          std::memcpy(buffer.data(), memory_data2.data(), buffer.size());
          return true;
        }
        // Individual address reads for UpdateSnapshotValues
        // Just calculate the offset and copy from the appropriate vector
        if (buffer.size() == sizeof(uint64_t)) {
          if (addr >= 0x1000 && addr < 0x2000) {
            size_t offset = addr - 0x1000;
            if (offset + sizeof(uint64_t) <= memory_data1.size()) {
              std::memcpy(
                  buffer.data(), &memory_data1[offset], sizeof(uint64_t));
              return true;
            }
          } else if (addr >= 0x2000 && addr < 0x3000) {
            size_t offset = addr - 0x2000;
            if (offset + sizeof(uint64_t) <= memory_data2.size()) {
              std::memcpy(
                  buffer.data(), &memory_data2[offset], sizeof(uint64_t));
              return true;
            }
          }
        }
        // Should never reach here if our math is correct
        return false;
      });

  auto params = MakeScanParams<uint64_t>(ScanComparison::kNotEqual, 100);
  auto result = scanner.NewScan(params);

  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 2);
  EXPECT_THAT(result.addresses(), ElementsAre(0x1008, 0x1018));

  std::vector<uint64_t> values;
  std::ranges::copy(result.values<uint64_t>(), std::back_inserter(values));
  EXPECT_THAT(values, ElementsAre(200, 300));
}

TEST_F(MemoryScannerTest, NextScanChangedValues) {
  MemoryScanner scanner(process_);

  // First scan - setup initial values
  std::vector<std::byte> first_memory(0x1000);
  uint32_t* first_ints = reinterpret_cast<uint32_t*>(first_memory.data());
  first_ints[0] = 10;  // at 0x1000
  first_ints[1] = 20;  // at 0x1004
  first_ints[2] = 30;  // at 0x1008

  // Mock all ReadMemory calls for first scan with a single expectation
  EXPECT_CALL(process_, ReadMemory(testing::_, testing::_))
      .WillRepeatedly([&first_memory, &first_ints](
                          uintptr_t addr, std::span<std::byte> buffer) {
        // Region read (large buffer)
        if (addr == 0x1000 && buffer.size() == 0x1000) {
          std::memcpy(buffer.data(), first_memory.data(), buffer.size());
          return true;
        }
        // Region read for second region
        if (addr == 0x2000 && buffer.size() == 0x1000) {
          std::fill(buffer.begin(), buffer.end(), std::byte{0});
          return true;
        }
        // Individual address reads for UpdateSnapshotValues
        if (buffer.size() == sizeof(uint32_t) && addr == 0x1004) {
          std::memcpy(buffer.data(), &first_ints[1], sizeof(uint32_t));
          return true;
        }
        // Default: fill with zeros
        std::fill(buffer.begin(), buffer.end(), std::byte{0});
        return true;
      });

  auto first_params = MakeScanParams<uint32_t>(ScanComparison::kExactValue, 20);
  auto first_result = scanner.NewScan(first_params);

  EXPECT_EQ(first_result.size(), 1);
  EXPECT_THAT(first_result.addresses(), ElementsAre(0x1004));

  // Second scan - mock the changed value read
  EXPECT_CALL(process_, ReadMemory(0x1004, testing::_))
      .WillOnce([](uintptr_t, std::span<std::byte> buffer) {
        uint32_t changed_value = 25;  // changed from 20
        std::memcpy(buffer.data(), &changed_value, sizeof(uint32_t));
        return true;
      });

  auto second_params = MakeScanParams<uint32_t>(ScanComparison::kChanged, 0);
  auto second_result = scanner.NextScan(first_result, second_params);

  // Should find addresses where values changed
  EXPECT_FALSE(second_result.empty());
  EXPECT_EQ(second_result.size(), 1);
  EXPECT_THAT(second_result.addresses(), ElementsAre(0x1004));
}

TEST_F(MemoryScannerTest, EmptyResultWhenNoMatches) {
  MemoryScanner scanner(process_);

  std::vector<std::byte> memory_data(0x1000);
  uint32_t* ints = reinterpret_cast<uint32_t*>(memory_data.data());
  ints[0] = 10;
  ints[1] = 20;
  ints[2] = 30;

  // Mock region reads for scanning
  EXPECT_CALL(process_, ReadMemory(0x1000, testing::_))
      .WillOnce([&memory_data](uintptr_t, std::span<std::byte> buffer) {
        std::memcpy(buffer.data(), memory_data.data(), buffer.size());
        return true;
      });

  EXPECT_CALL(process_, ReadMemory(0x2000, testing::_))
      .WillOnce([](uintptr_t, std::span<std::byte> buffer) {
        std::fill(buffer.begin(), buffer.end(), std::byte{0});
        return true;
      });

  // No individual address reads expected since no matches found

  auto params = MakeScanParams<uint32_t>(ScanComparison::kExactValue, 999);
  auto result = scanner.NewScan(params);

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.size(), 0);
}

TEST_F(MemoryScannerTest, InvalidProcessReturnsEmpty) {
  EXPECT_CALL(process_, IsProcessValid()).WillRepeatedly(Return(false));

  MemoryScanner scanner(process_);
  auto params = MakeScanParams<uint32_t>(ScanComparison::kExactValue, 42);
  auto result = scanner.NewScan(params);

  EXPECT_TRUE(result.empty());
}

}  // namespace
}  // namespace maia
