// Copyright (c) Maia

#include "maia/core/simd_scanner.h"

#include <gtest/gtest.h>

#include <vector>

namespace maia::core {

class SimdScannerTest : public ::testing::Test {
 protected:
  std::vector<size_t> found_offsets_;

  void OnMatch(size_t offset) {
    found_offsets_.push_back(offset);
  }

  void Clear() {
    found_offsets_.clear();
  }
};

TEST_F(SimdScannerTest, ScalarFallbackUsedForShortBuffers) {
  // Buffer shorter than 32 bytes (AVX2 width)
  std::vector<std::byte> buffer(10);
  std::fill(buffer.begin(), buffer.end(), std::byte{0});
  buffer[5] = std::byte{0xFF};

  std::vector<std::byte> pattern;
  pattern.push_back(std::byte{0xFF});

  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 5);
}

TEST_F(SimdScannerTest, FindsSingleMatchInLargeBuffer) {
  // 64 bytes buffer (2 x AVX2 width)
  std::vector<std::byte> buffer(64);
  std::fill(buffer.begin(), buffer.end(), std::byte{0});
  buffer[40] = std::byte{0xAA};  // Match in second 32-byte block

  std::vector<std::byte> pattern;
  pattern.push_back(std::byte{0xAA});

  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 40);
}

TEST_F(SimdScannerTest, FindsMultipleMatches) {
  std::vector<std::byte> buffer(100);
  std::fill(buffer.begin(), buffer.end(), std::byte{0});
  buffer[10] = std::byte{0xBB};
  buffer[50] = std::byte{0xBB};
  buffer[90] = std::byte{0xBB};

  std::vector<std::byte> pattern;
  pattern.push_back(std::byte{0xBB});

  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 3);
  EXPECT_EQ(found_offsets_[0], 10);
  EXPECT_EQ(found_offsets_[1], 50);
  EXPECT_EQ(found_offsets_[2], 90);
}

TEST_F(SimdScannerTest, FindsPatternCrossingAvxBoundary) {
  // Buffer of 64 bytes.
  // Boundary is at index 32.
  // We place a 4-byte pattern at index 30: [30, 31, 32, 33]
  std::vector<std::byte> buffer(64);
  std::fill(buffer.begin(), buffer.end(), std::byte{0});

  std::vector<std::byte> pattern;
  pattern.push_back(std::byte{0x1});
  pattern.push_back(std::byte{0x2});
  pattern.push_back(std::byte{0x3});
  pattern.push_back(std::byte{0x4});

  buffer[30] = std::byte{0x1};
  buffer[31] = std::byte{0x2};
  buffer[32] = std::byte{0x3};
  buffer[33] = std::byte{0x4};

  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 30);
}

TEST_F(SimdScannerTest, FindsMatchAtVeryEnd) {
  std::vector<std::byte> buffer(40);
  std::fill(buffer.begin(), buffer.end(), std::byte{0});

  // Pattern at the very last byte
  buffer[39] = std::byte{0xCC};

  std::vector<std::byte> pattern;
  pattern.push_back(std::byte{0xCC});

  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 39);
}

TEST_F(SimdScannerTest, RespectsLongPattern) {
  std::vector<std::byte> buffer(64);
  std::fill(buffer.begin(), buffer.end(), std::byte{0});

  // "False positive" partial match
  buffer[10] = std::byte{0xAA};
  buffer[11] = std::byte{0xBB};
  buffer[12] = std::byte{0x00};  // mismatch

  // Real match
  buffer[20] = std::byte{0xAA};
  buffer[21] = std::byte{0xBB};
  buffer[22] = std::byte{0xCC};

  std::vector<std::byte> pattern;
  pattern.push_back(std::byte{0xAA});
  pattern.push_back(std::byte{0xBB});
  pattern.push_back(std::byte{0xCC});

  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 20);
}

TEST_F(SimdScannerTest, ScanMemCmpFindsEquality) {
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  // Create mismatches
  buf1[10] = std::byte{1};
  buf2[10] = std::byte{2};

  buf1[50] = std::byte{1};
  buf2[50] = std::byte{2};

  // We look for EQUAL regions.
  // Stride 1.
  // Indices 10 and 50 are NOT equal. All others are equal.
  // We expect 62 matches.

  size_t count = 0;
  ScanMemCmp(buf1, buf2, true, 1, [&](size_t) { count++; });
  EXPECT_EQ(count, 62);
}

TEST_F(SimdScannerTest, ScanMemCmpFindsInequality) {
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  // Mismatches at 10 and 50
  buf1[10] = std::byte{1};
  buf2[10] = std::byte{2};

  buf1[50] = std::byte{1};
  buf2[50] = std::byte{2};

  ScanMemCmp(buf1, buf2, false, 1, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 10);
  EXPECT_EQ(found_offsets_[1], 50);
}

TEST_F(SimdScannerTest, ScanMemCmpRespectsStride) {
  // Stride 4 (Int32)
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  // Offset 4: mismatch in 1 byte -> Value Mismatch
  buf1[4] = std::byte{1};
  buf2[4] = std::byte{2};

  // Offset 8: match
  // Offset 12: match

  // Offset 16: mismatch in 2nd byte of stride
  buf1[17] = std::byte{1};
  buf2[17] = std::byte{2};

  // Find Changed (Inequality)
  ScanMemCmp(buf1, buf2, false, 4, [this](size_t o) { OnMatch(o); });

  // Should find offset 4 and 16.
  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 4);
  EXPECT_EQ(found_offsets_[1], 16);
}

}  // namespace maia::core
