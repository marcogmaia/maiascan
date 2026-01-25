// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
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

  ScanResultModel model_;
  std::unique_ptr<LargeFakeProcess> process_;
};

TEST_F(ScanResultModelChunkedTest, FindsMatchCrossingChunkBoundary) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB

  // Place a 4-byte value crossing the chunk boundary.
  // Boundary is at offset 32MB.
  // Offset = 32MB - 2 bytes means bytes [32MB-2, 32MB+1] cross the boundary.
  const size_t boundary_offset = kChunkSize - 2;
  const uint32_t magic_value = 0xDEADBEEF;

  process_->WriteValue<uint32_t>(boundary_offset, magic_value);

  // Also place values well before and well after the boundary
  process_->WriteValue<uint32_t>(100, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_.FirstScan();

  const auto& storage = model_.entries();

  // Should find all 3 matches
  ASSERT_EQ(storage.addresses.size(), 3);

  bool found_boundary = false;
  uintptr_t base = process_->GetBaseAddress();

  for (auto addr : storage.addresses) {
    if (addr == base + boundary_offset) {
      found_boundary = true;
    }
  }

  EXPECT_TRUE(found_boundary)
      << "Failed to find match crossing 32MB chunk boundary!";
}

TEST_F(ScanResultModelChunkedTest, UnknownScanSnapshotsLargeRegion) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB

  // Write distinct values at specific locations
  process_->WriteValue<uint32_t>(0, 0x11111111);
  process_->WriteValue<uint32_t>(kChunkSize, 0x22222222);
  process_->WriteValue<uint32_t>(kChunkSize + 100, 0x33333333);

  model_.SetScanComparison(ScanComparison::kUnknown);

  model_.FirstScan();

  const auto& storage = model_.entries();

  // 40MB / 4 bytes = 10 million addresses
  // With alignment, we expect around 10M entries
  EXPECT_GT(storage.addresses.size(), 1000000)
      << "Should snapshot millions of addresses for a 40MB region";

  // Verify the stride is correct
  EXPECT_EQ(storage.stride, sizeof(uint32_t));
}

}  // namespace
}  // namespace maia
