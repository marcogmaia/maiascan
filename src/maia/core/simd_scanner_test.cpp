// Copyright (c) Maia

#include "maia/core/simd_scanner.h"

#include <gtest/gtest.h>

#include <vector>

namespace maia::core {

class SimdScannerTest : public ::testing::Test {
 protected:
  std::vector<size_t> found_offsets_;

  void OnMatch(size_t offset) {
    found_offsets_.emplace_back(offset);
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
  std::vector<std::byte> buffer(40, std::byte{0});

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

TEST_F(SimdScannerTest, ScanMemCmpLargeStride) {
  // Stride 8 (Int64/Double)
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  buf1[8] = std::byte{1};
  buf2[8] = std::byte{2};

  ScanMemCmp(buf1, buf2, false, 8, [this](size_t o) { OnMatch(o); });
  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 8);

  // Stride 16
  Clear();
  buf1[33] = std::byte{1};
  buf2[34] = std::byte{2};
  ScanMemCmp(buf1, buf2, false, 16, [this](size_t o) { OnMatch(o); });
  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 0);
  EXPECT_EQ(found_offsets_[1], 32);
}

TEST_F(SimdScannerTest, TailLogicBoundaries) {
  for (size_t size : {31, 32, 33}) {
    Clear();
    std::vector<std::byte> buffer(size, std::byte{0});
    buffer[size - 1] = std::byte{0xFF};
    std::vector<std::byte> pattern = {std::byte{0xFF}};

    ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });
    ASSERT_EQ(found_offsets_.size(), 1) << "Failed for size " << size;
    EXPECT_EQ(found_offsets_[0], size - 1);
  }
}

TEST_F(SimdScannerTest, ScanMemCompareGreaterInt32) {
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  // 100 > 50 at offset 4
  int32_t val1 = 100;
  int32_t val2 = 50;
  std::memcpy(buf1.data() + 4, &val1, 4);
  std::memcpy(buf2.data() + 4, &val2, 4);

  // 50 < 100 at offset 12 (should not match)
  val1 = 50;
  val2 = 100;
  std::memcpy(buf1.data() + 12, &val1, 4);
  std::memcpy(buf2.data() + 12, &val2, 4);

  // 200 > 100 at offset 40
  val1 = 200;
  val2 = 100;
  std::memcpy(buf1.data() + 40, &val1, 4);
  std::memcpy(buf2.data() + 40, &val2, 4);

  ScanMemCompareGreater<int32_t>(buf1, buf2, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 4);
  EXPECT_EQ(found_offsets_[1], 40);
}

TEST_F(SimdScannerTest, ScanMemCompareGreaterFloat) {
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  float val1 = 100.5f;
  float val2 = 100.4f;
  std::memcpy(buf1.data() + 8, &val1, 4);
  std::memcpy(buf2.data() + 8, &val2, 4);

  ScanMemCompareGreater<float>(buf1, buf2, [this](size_t o) { OnMatch(o); });
  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 8);
}

TEST_F(SimdScannerTest, ScanMemCompareGreaterScalarFallback) {
  // Test with double (8 bytes), which should use scalar fallback
  std::vector<std::byte> buf1(64, std::byte{0});
  std::vector<std::byte> buf2(64, std::byte{0});

  double val1 = 500.0;
  double val2 = 250.0;
  std::memcpy(buf1.data() + 16, &val1, 8);
  std::memcpy(buf2.data() + 16, &val2, 8);

  ScanMemCompareGreater<double>(buf1, buf2, [this](size_t o) { OnMatch(o); });
  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 16);
}

TEST_F(SimdScannerTest, ScanBufferRespectsAlignment4) {
  // 64 bytes buffer, pattern at multiple offsets
  std::vector<std::byte> buffer(64, std::byte{0});

  // Place pattern at aligned offsets only (no overlaps)
  uint32_t pattern_val = 0xDEADBEEF;
  std::memcpy(buffer.data() + 8, &pattern_val, 4);   // Aligned
  std::memcpy(buffer.data() + 40, &pattern_val, 4);  // Aligned

  // Place pattern at unaligned offset 17 (won't overlap with 8 or 40)
  std::memcpy(buffer.data() + 17, &pattern_val, 4);  // Unaligned

  std::vector<std::byte> pattern(4);
  std::memcpy(pattern.data(), &pattern_val, 4);

  // With alignment=4, should only find offsets 8 and 40 (not 17)
  ScanBuffer(buffer, pattern, 4, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 8);
  EXPECT_EQ(found_offsets_[1], 40);
}

TEST_F(SimdScannerTest, ScanBufferAlignment1FindsAll) {
  // Same setup as above, but alignment=1 should find all matches
  Clear();
  std::vector<std::byte> buffer(64, std::byte{0});

  uint32_t pattern_val = 0xDEADBEEF;
  std::memcpy(buffer.data() + 8, &pattern_val, 4);
  std::memcpy(buffer.data() + 17, &pattern_val, 4);  // Unaligned, no overlap
  std::memcpy(buffer.data() + 40, &pattern_val, 4);

  std::vector<std::byte> pattern(4);
  std::memcpy(pattern.data(), &pattern_val, 4);

  // With alignment=1 (default), should find all 3 matches
  ScanBuffer(buffer, pattern, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 3);
  EXPECT_EQ(found_offsets_[0], 8);
  EXPECT_EQ(found_offsets_[1], 17);
  EXPECT_EQ(found_offsets_[2], 40);
}

TEST_F(SimdScannerTest, ScanBufferAlignment8) {
  Clear();
  std::vector<std::byte> buffer(64, std::byte{0});

  uint64_t pattern_val = 0xDEADBEEFCAFEBABE;
  std::memcpy(buffer.data() + 0, &pattern_val, 8);   // Aligned to 8
  std::memcpy(buffer.data() + 24, &pattern_val, 8);  // Aligned to 8
  std::memcpy(buffer.data() + 13, &pattern_val, 8);  // Not aligned to 8

  std::vector<std::byte> pattern(8);
  std::memcpy(pattern.data(), &pattern_val, 8);

  ScanBuffer(buffer, pattern, 8, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 0);
  EXPECT_EQ(found_offsets_[1], 24);
}

TEST_F(SimdScannerTest, ScanBufferAlignment2ForInt16) {
  Clear();
  std::vector<std::byte> buffer(64, std::byte{0});

  uint16_t pattern_val = 0xBEEF;
  std::memcpy(buffer.data() + 0, &pattern_val, 2);   // Aligned to 2
  std::memcpy(buffer.data() + 6, &pattern_val, 2);   // Aligned to 2
  std::memcpy(buffer.data() + 11, &pattern_val, 2);  // NOT aligned to 2
  std::memcpy(buffer.data() + 20, &pattern_val, 2);  // Aligned to 2

  std::vector<std::byte> pattern(2);
  std::memcpy(pattern.data(), &pattern_val, 2);

  ScanBuffer(buffer, pattern, 2, [this](size_t o) { OnMatch(o); });

  // Should find 0, 6, 20 but NOT 11
  ASSERT_EQ(found_offsets_.size(), 3);
  EXPECT_EQ(found_offsets_[0], 0);
  EXPECT_EQ(found_offsets_[1], 6);
  EXPECT_EQ(found_offsets_[2], 20);
}

TEST_F(SimdScannerTest, ScanBufferUnalignedSkipped) {
  // Explicit test that unaligned matches are skipped
  Clear();
  std::vector<std::byte> buffer(64, std::byte{0});

  uint32_t pattern_val = 0xCAFEBABE;
  // Place pattern ONLY at unaligned offsets
  std::memcpy(buffer.data() + 1, &pattern_val, 4);   // Unaligned
  std::memcpy(buffer.data() + 7, &pattern_val, 4);   // Unaligned
  std::memcpy(buffer.data() + 13, &pattern_val, 4);  // Unaligned

  std::vector<std::byte> pattern(4);
  std::memcpy(pattern.data(), &pattern_val, 4);

  ScanBuffer(buffer, pattern, 4, [this](size_t o) { OnMatch(o); });

  // Should find NOTHING because all matches are unaligned
  EXPECT_EQ(found_offsets_.size(), 0);
}

TEST_F(SimdScannerTest, ScanBufferScalarFallbackRespectsAlignment) {
  // Test that the scalar fallback (buffer < 32 bytes) also respects alignment
  Clear();
  std::vector<std::byte> buffer(20, std::byte{0});  // < 32 bytes, uses scalar

  uint32_t pattern_val = 0xDEADBEEF;
  std::memcpy(buffer.data() + 0, &pattern_val, 4);   // Aligned
  std::memcpy(buffer.data() + 5, &pattern_val, 4);   // Unaligned
  std::memcpy(buffer.data() + 12, &pattern_val, 4);  // Aligned

  std::vector<std::byte> pattern(4);
  std::memcpy(pattern.data(), &pattern_val, 4);

  ScanBuffer(buffer, pattern, 4, [this](size_t o) { OnMatch(o); });

  // Should find 0 and 12, but NOT 5
  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 0);
  EXPECT_EQ(found_offsets_[1], 12);
}

TEST_F(SimdScannerTest, ScanBufferMaskedSimple) {
  std::vector<std::byte> buffer(64, std::byte{0});
  buffer[10] = std::byte{0xAA};
  buffer[11] = std::byte{0xBB};
  buffer[12] = std::byte{0xCC};

  buffer[40] = std::byte{0xAA};
  buffer[41] = std::byte{0xDD};  // Wildcard match here
  buffer[42] = std::byte{0xCC};

  std::vector<std::byte> pattern = {
      std::byte{0xAA}, std::byte{0x00}, std::byte{0xCC}};
  std::vector<std::byte> mask = {
      std::byte{0xFF}, std::byte{0x00}, std::byte{0xFF}};

  ScanBufferMasked(buffer, pattern, mask, [this](size_t o) { OnMatch(o); });

  ASSERT_EQ(found_offsets_.size(), 2);
  EXPECT_EQ(found_offsets_[0], 10);
  EXPECT_EQ(found_offsets_[1], 40);
}

TEST_F(SimdScannerTest, ScanBufferMaskedFirstByteWildcard) {
  std::vector<std::byte> buffer(64, std::byte{0x11});
  buffer[10] = std::byte{0xAA};
  buffer[11] = std::byte{0xBB};

  std::vector<std::byte> pattern = {std::byte{0x00}, std::byte{0xBB}};
  std::vector<std::byte> mask = {std::byte{0x00}, std::byte{0xFF}};

  // This should trigger the non-v_first optimization path (scalar or fallback)
  ScanBufferMasked(buffer, pattern, mask, [this](size_t o) { OnMatch(o); });

  // Should find many matches since first byte is ignored and 0xBB is at 11
  // and all other bytes are 0x11 (but only offset 10 matches the 0xBB at offset
  // + 1)
  ASSERT_EQ(found_offsets_.size(), 1);
  EXPECT_EQ(found_offsets_[0], 10);
}

}  // namespace maia::core
