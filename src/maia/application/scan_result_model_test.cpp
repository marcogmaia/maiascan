// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "maia/core/i_process.h"

namespace maia {
namespace {

class FakeProcess : public IProcess {
 public:
  explicit FakeProcess(size_t memory_size = 4096) {
    memory_.resize(memory_size, std::byte{0});
    base_address_ = 0x100000;
  }

  template <typename T>
  void WriteValue(size_t offset, T value) {
    if (offset + sizeof(T) > memory_.size()) {
      return;
    }
    std::memcpy(&memory_[offset], &value, sizeof(T));
  }

  std::vector<std::byte>& GetRawMemory() {
    return memory_;
  }

  bool ReadMemory(std::span<const MemoryAddress> addresses,
                  size_t bytes_per_address,
                  std::span<std::byte> out_buffer) override {
    if (!is_valid_) {
      return false;
    }

    if (out_buffer.size() < addresses.size() * bytes_per_address) {
      return false;
    }

    // Case 1: Single continuous read (First Scan optimization)
    if (addresses.size() == 1) {
      uintptr_t addr = addresses[0];
      if (addr < base_address_) {
        return false;
      }
      size_t offset = addr - base_address_;

      if (offset + bytes_per_address > memory_.size()) {
        return false;
      }
      std::memcpy(out_buffer.data(), &memory_[offset], bytes_per_address);
      return true;
    }

    // Case 2: Scatter/Gather Read (Next Scan)
    std::byte* out_ptr = out_buffer.data();
    for (const auto& addr : addresses) {
      if (addr < base_address_) {
        std::memset(out_ptr, 0, bytes_per_address);
      } else {
        size_t offset = addr - base_address_;
        if (offset + bytes_per_address > memory_.size()) {
          std::memset(out_ptr, 0, bytes_per_address);
        } else {
          std::memcpy(out_ptr, &memory_[offset], bytes_per_address);
        }
      }
      out_ptr += bytes_per_address;
    }

    return true;
  }

  bool WriteMemory(uintptr_t, std::span<const std::byte>) override {
    return true;
  }

  std::vector<MemoryRegion> GetMemoryRegions() const override {
    if (!is_valid_) {
      return {};
    }
    MemoryRegion region;
    region.base = base_address_;
    region.size = memory_.size();
    region.protection = mmem::Protection::kReadWrite;
    return {region};
  }

  uint32_t GetProcessId() const override {
    return 1234;
  }

  std::string GetProcessName() const override {
    return "test_app.exe";
  }

  bool IsProcessValid() const override {
    return is_valid_;
  }

  uintptr_t GetBaseAddress() const override {
    return base_address_;
  }

  void SetValid(bool valid) {
    is_valid_ = valid;
  }

 private:
  std::vector<std::byte> memory_;
  uintptr_t base_address_;
  bool is_valid_ = true;
};

class ScanResultModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    process_ = std::make_unique<FakeProcess>(1024);
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
  std::unique_ptr<FakeProcess> process_;
};

TEST_F(ScanResultModelTest, FirstScanExactValueFindsMatches) {
  process_->WriteValue<uint32_t>(100, 42);
  process_->WriteValue<uint32_t>(200, 99);
  process_->WriteValue<uint32_t>(500, 42);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(42));

  model_.FirstScan();

  const auto& storage = model_.entries();
  ASSERT_EQ(storage.addresses.size(), 2);
  EXPECT_EQ(storage.addresses[0], 0x100000 + 100);
  EXPECT_EQ(storage.addresses[1], 0x100000 + 500);

  EXPECT_EQ(storage.stride, 4);
  uint32_t val1 = *reinterpret_cast<const uint32_t*>(storage.curr_raw.data());
  EXPECT_EQ(val1, 42);
}

TEST_F(ScanResultModelTest, FirstScanUnknownValueSnapshotsMemory) {
  process_->WriteValue<uint32_t>(0, 10);
  model_.SetScanComparison(ScanComparison::kUnknown);

  model_.FirstScan();

  const auto& entries = model_.entries();
  // 1KB / 4 bytes = ~256 potential alignments
  EXPECT_GT(entries.addresses.size(), 250);

  uint32_t val0 = *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
  EXPECT_EQ(val0, 10);
}

TEST_F(ScanResultModelTest, NextScanIncreasedValueFiltersResults) {
  process_->WriteValue<uint32_t>(100, 10);
  process_->WriteValue<uint32_t>(200, 50);

  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();

  process_->WriteValue<uint32_t>(100, 15);  // Increased

  model_.SetScanComparison(ScanComparison::kIncreased);
  model_.NextScan();

  const auto& entries = model_.entries();

  bool found_100 = false;
  bool found_200 = false;

  for (const auto addr : entries.addresses) {
    if (addr == 0x100000 + 100) {
      found_100 = true;
    }
    if (addr == 0x100000 + 200) {
      found_200 = true;
    }
  }

  EXPECT_TRUE(found_100);
  EXPECT_FALSE(found_200);
}

TEST_F(ScanResultModelTest, NextScanExactValueFiltersResults) {
  process_->WriteValue<uint32_t>(10, 100);
  process_->WriteValue<uint32_t>(20, 100);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(100));
  model_.FirstScan();

  ASSERT_EQ(model_.entries().addresses.size(), 2);

  process_->WriteValue<uint32_t>(20, 101);

  model_.NextScan();

  const auto& entries = model_.entries();
  ASSERT_EQ(entries.addresses.size(), 1);
  EXPECT_EQ(entries.addresses[0], 0x100000 + 10);
}

TEST_F(ScanResultModelTest, SignalEmittedOnScan) {
  struct TestListener {
    bool signal_received = false;
    size_t received_count = 0;

    void OnMemoryChanged(const ScanStorage& s) {
      signal_received = true;
      received_count = s.addresses.size();
    }
  };

  TestListener listener;

  entt::scoped_connection conn =
      model_.sinks().MemoryChanged().connect<&TestListener::OnMemoryChanged>(
          listener);

  process_->WriteValue<uint32_t>(10, 999);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(999));

  model_.FirstScan();

  EXPECT_TRUE(listener.signal_received);
  EXPECT_EQ(listener.received_count, 1);
}

TEST_F(ScanResultModelTest, InvalidProcessDoesNothing) {
  process_->SetValid(false);
  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  EXPECT_TRUE(model_.entries().addresses.empty());
}

TEST_F(ScanResultModelTest, ClearResetsStorage) {
  process_->WriteValue<uint32_t>(0, 123);
  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  ASSERT_FALSE(model_.entries().addresses.empty());

  model_.Clear();

  EXPECT_TRUE(model_.entries().addresses.empty());
  EXPECT_TRUE(model_.entries().curr_raw.empty());
}

TEST_F(ScanResultModelTest, NextScanPopulatesPreviousValues) {
  process_->WriteValue<uint32_t>(100, 10);

  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();

  process_->WriteValue<uint32_t>(100, 20);
  model_.SetScanComparison(ScanComparison::kChanged);
  model_.NextScan();

  const auto& entries = model_.entries();
  ASSERT_FALSE(entries.addresses.empty());

  ASSERT_EQ(entries.prev_raw.size(), entries.curr_raw.size())
      << "Previous raw buffer should be same size as current raw buffer";

  uint32_t prev_val =
      *reinterpret_cast<const uint32_t*>(entries.prev_raw.data());
  uint32_t curr_val =
      *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
  EXPECT_EQ(prev_val, 10);
  EXPECT_EQ(curr_val, 20);
}

}  // namespace
}  // namespace maia
