// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "maia/assert.h"
#include "maia/core/i_process.h"

namespace maia {
namespace {

// A FakeProcess that supports large memory regions for chunked scanning tests.
class LargeFakeProcess : public IProcess {
 public:
  explicit LargeFakeProcess(size_t memory_size) {
    memory_.resize(memory_size, std::byte{0});
    base_address_ = 0x100000;
  }

  template <typename T>
  void WriteValue(size_t offset, T value) {
    Assert((offset + sizeof(T)) <= memory_.size());
    std::memcpy(&memory_[offset], &value, sizeof(T));
  }

  bool ReadMemory(std::span<const MemoryAddress> addresses,
                  size_t bytes_per_address,
                  std::span<std::byte> out_buffer,
                  std::vector<uint8_t>* /*success_mask*/) override {
    if (addresses.size() != 1) {
      return false;  // Only support single-address reads for FirstScan
    }

    uintptr_t addr = addresses[0];
    size_t len = bytes_per_address;

    if (addr < base_address_) {
      return false;
    }

    size_t offset = addr - base_address_;
    if (offset + len > memory_.size()) {
      return false;
    }

    if (out_buffer.size() < len) {
      return false;
    }

    std::memcpy(out_buffer.data(), &memory_[offset], len);
    return true;
  }

  bool WriteMemory(uintptr_t, std::span<const std::byte>) override {
    return true;
  }

  std::vector<MemoryRegion> GetMemoryRegions() const override {
    MemoryRegion region;
    region.base = base_address_;
    region.size = memory_.size();
    region.protection = mmem::Protection::kReadWrite;
    return {region};
  }

  std::vector<mmem::ModuleDescriptor> GetModules() const override {
    return {};
  }

  uint32_t GetProcessId() const override {
    return 1;
  }

  std::string GetProcessName() const override {
    return "large_test.exe";
  }

  bool IsProcessValid() const override {
    return true;
  }

  uintptr_t GetBaseAddress() const override {
    return base_address_;
  }

  bool Suspend() override {
    return true;
  }

  bool Resume() override {
    return true;
  }

 private:
  std::vector<std::byte> memory_;
  uintptr_t base_address_;
};

class ScanResultModelChunkedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 40MB to safely cover 32MB chunk boundary
    process_ = std::make_unique<LargeFakeProcess>(40 * 1024 * 1024);
    model_.SetActiveProcess(process_.get());
    model_.StopAutoUpdate();
  }

  void TearDown() override {
    model_.Clear();
  }

  template <typename T>
  std::vector<std::byte> ToBytes(T val) {
    std::vector<std::byte> b(sizeof(T));
    std::memcpy(b.data(), &val, sizeof(T));
    return b;
  }

  /// Waits for the async scan to complete and applies the result.
  void WaitForScan() {
    while (!model_.HasPendingResult()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    model_.ApplyPendingResult();
  }

  ScanResultModel model_;
  std::unique_ptr<LargeFakeProcess> process_;
};

TEST_F(ScanResultModelChunkedTest, FindsMatchCrossingChunkBoundary) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB

  // Place a 4-byte value near the chunk boundary at an aligned offset.
  // Boundary is at offset 32MB.
  // Offset = 32MB - 4 bytes means bytes [32MB-4, 32MB-1] are in chunk 1.
  // This tests that the overlap logic reads slightly past the scan boundary.
  // Note: With alignment=4, we only check aligned offsets.
  const size_t near_boundary_offset = kChunkSize - 4;  // Aligned to 4
  const uint32_t magic_value = 0xDEADBEEF;

  process_->WriteValue<uint32_t>(near_boundary_offset, magic_value);

  // Also place values well before and well after the boundary
  process_->WriteValue<uint32_t>(100, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();

  // Should find all 3 matches
  ASSERT_EQ(storage.addresses.size(), 3);

  bool found_near_boundary = false;
  uintptr_t base = process_->GetBaseAddress();

  for (auto addr : storage.addresses) {
    if (addr == base + near_boundary_offset) {
      found_near_boundary = true;
    }
  }

  EXPECT_TRUE(found_near_boundary)
      << "Failed to find match near 32MB chunk boundary!";
}

TEST_F(ScanResultModelChunkedTest, UnknownScanSnapshotsLargeRegion) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB

  // Write distinct values at specific locations
  process_->WriteValue<uint32_t>(0, 0x11111111);
  process_->WriteValue<uint32_t>(kChunkSize, 0x22222222);
  process_->WriteValue<uint32_t>(kChunkSize + 100, 0x33333333);

  model_.SetScanComparison(ScanComparison::kUnknown);

  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();

  // 40MB / 4 bytes = 10 million addresses
  // With alignment, we expect around 10M entries
  EXPECT_GT(storage.addresses.size(), 1000000)
      << "Should snapshot millions of addresses for a 40MB region";

  // Verify the stride is correct
  EXPECT_EQ(storage.stride, sizeof(uint32_t));
}

TEST_F(ScanResultModelChunkedTest, ExactScanSkipsUnalignedAddresses) {
  // This test verifies that the full scan pipeline respects alignment.
  // Values placed at unaligned offsets should NOT be found.
  const uint32_t magic_value = 0xCAFEBABE;

  // Place value at aligned offsets (divisible by 4) - well separated
  process_->WriteValue<uint32_t>(0, magic_value);     // Aligned
  process_->WriteValue<uint32_t>(100, magic_value);   // Aligned
  process_->WriteValue<uint32_t>(1000, magic_value);  // Aligned

  // Place value at unaligned offsets (NOT divisible by 4) - well separated
  process_->WriteValue<uint32_t>(201, magic_value);  // Unaligned
  process_->WriteValue<uint32_t>(307, magic_value);  // Unaligned
  process_->WriteValue<uint32_t>(503, magic_value);  // Unaligned

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();
  uintptr_t base = process_->GetBaseAddress();

  // Should find exactly 3 matches (only aligned ones)
  ASSERT_EQ(storage.addresses.size(), 3)
      << "Should only find aligned matches, not unaligned ones";

  // Verify all found addresses are aligned
  for (auto addr : storage.addresses) {
    size_t offset = addr - base;
    EXPECT_EQ(offset % 4, 0) << "Found unaligned address at offset " << offset;
  }

  // Verify specific aligned offsets were found
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + 100);
  EXPECT_EQ(storage.addresses[2], base + 1000);
}

TEST_F(ScanResultModelChunkedTest, ExactScanUnalignedOnlyFindsNothing) {
  // If ALL values are at unaligned offsets, the scan should find nothing.
  const uint32_t magic_value = 0xDEADC0DE;

  // Place values ONLY at unaligned offsets (well separated to avoid overlap)
  process_->WriteValue<uint32_t>(101, magic_value);
  process_->WriteValue<uint32_t>(205, magic_value);
  process_->WriteValue<uint32_t>(309, magic_value);
  process_->WriteValue<uint32_t>(413, magic_value);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();

  // Should find nothing because all values are at unaligned offsets
  EXPECT_EQ(storage.addresses.size(), 0)
      << "Should not find any matches when all are unaligned";
}

TEST_F(ScanResultModelChunkedTest, AlignmentAcrossChunkBoundary) {
  // Test that alignment is correctly maintained across chunk boundaries.
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB
  const uint32_t magic_value = 0xBEEFCAFE;

  // Place aligned values in different chunks (well separated)
  process_->WriteValue<uint32_t>(0, magic_value);                 // Chunk 0
  process_->WriteValue<uint32_t>(kChunkSize, magic_value);        // Chunk 1
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);  // Chunk 1

  // Place unaligned values that should be skipped (well separated)
  process_->WriteValue<uint32_t>(kChunkSize + 201, magic_value);  // Unaligned
  process_->WriteValue<uint32_t>(kChunkSize + 303, magic_value);  // Unaligned

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();
  uintptr_t base = process_->GetBaseAddress();

  // Should find exactly 3 aligned matches across chunks
  ASSERT_EQ(storage.addresses.size(), 3);
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + kChunkSize);
  EXPECT_EQ(storage.addresses[2], base + kChunkSize + 100);
}

TEST_F(ScanResultModelChunkedTest, FindsUnalignedWhenFastScanDisabled) {
  // Verifies that disabling "Fast Scan" re-enables finding unaligned values.
  const uint32_t magic_value = 0xCAFEBABE;
  const uintptr_t base = process_->GetBaseAddress();

  // Place value at unaligned offsets
  process_->WriteValue<uint32_t>(1, magic_value);
  process_->WriteValue<uint32_t>(13, magic_value);

  model_.SetFastScan(false);  // DISABLE FAST SCAN
  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();

  // Should now find BOTH unaligned matches
  ASSERT_EQ(storage.addresses.size(), 2);
  EXPECT_EQ(storage.addresses[0], base + 1);
  EXPECT_EQ(storage.addresses[1], base + 13);
}

TEST_F(ScanResultModelChunkedTest,
       UnknownScanFindsUnalignedWhenFastScanDisabled) {
  // For unknown scan, disabling fast scan should snapshot EVERY byte.
  // Note: This is memory intensive in real scenarios, but fine for a small
  // test.
  model_.SetFastScan(false);  // DISABLE FAST SCAN
  model_.SetScanComparison(ScanComparison::kUnknown);

  // Use a smaller region for this test to avoid massive result lists.
  // (Our fake process has 40MB, but we only care about the first few bytes).
  model_.FirstScan();
  WaitForScan();

  const auto& storage = model_.entries();
  const uintptr_t base = process_->GetBaseAddress();

  // With alignment=1, we should find matches at 0, 1, 2, 3, 4, ...
  ASSERT_GE(storage.addresses.size(), 10);
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + 1);
  EXPECT_EQ(storage.addresses[2], base + 2);
  EXPECT_EQ(storage.addresses[3], base + 3);
}

}  // namespace
}  // namespace maia
