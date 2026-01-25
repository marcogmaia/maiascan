// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <thread>
#include <unordered_set>
#include <vector>

#include "maia/assert.h"
#include "maia/core/i_process.h"

namespace maia {
namespace {

class FakeProcess : public IProcess {
 public:
  explicit FakeProcess(size_t memory_size = 0x1000) {
    memory_.resize(memory_size, std::byte{0});
    base_address_ = 0x100000;
  }

  template <typename T>
  void WriteValue(size_t offset, T value) {
    Assert((offset + sizeof(T)) <= memory_.size());
    std::memcpy(&memory_[offset], &value, sizeof(T));
  }

  void MarkAddressInvalid(uintptr_t addr) {
    invalid_addresses_.insert(addr);
  }

  std::vector<std::byte>& GetRawMemory() {
    return memory_;
  }

  bool ReadMemory(std::span<const MemoryAddress> addresses,
                  size_t bytes_per_address,
                  std::span<std::byte> out_buffer,
                  std::vector<uint8_t>* success_mask = nullptr) override {
    if (!is_valid_) {
      return false;
    }

    if (out_buffer.size() < addresses.size() * bytes_per_address) {
      return false;
    }

    bool all_success = true;

    // Single continuous read (First Scan optimization)
    if (addresses.size() == 1) {
      uintptr_t addr = addresses[0];
      if (success_mask && !success_mask->empty()) {
        (*success_mask)[0] = 1;
      }

      if (invalid_addresses_.contains(addr)) {
        if (success_mask && !success_mask->empty()) {
          (*success_mask)[0] = 0;
        }
        return false;
      }

      if (addr < base_address_) {
        if (success_mask && !success_mask->empty()) {
          (*success_mask)[0] = 0;
        }
        return false;
      }
      size_t offset = addr - base_address_;

      if (offset + bytes_per_address > memory_.size()) {
        if (success_mask && !success_mask->empty()) {
          (*success_mask)[0] = 0;
        }
        return false;
      }
      std::memcpy(out_buffer.data(), &memory_[offset], bytes_per_address);
      return true;
    }

    // Scatter/Gather Read (Next Scan)
    std::byte* out_ptr = out_buffer.data();
    for (size_t i = 0; i < addresses.size(); ++i) {
      const auto addr = addresses[i];
      bool success = true;

      if (invalid_addresses_.contains(addr)) {
        success = false;
        std::memset(out_ptr, 0, bytes_per_address);
      } else if (addr < base_address_) {
        success = false;
        std::memset(out_ptr, 0, bytes_per_address);
      } else {
        size_t offset = addr - base_address_;
        if (offset + bytes_per_address > memory_.size()) {
          success = false;
          std::memset(out_ptr, 0, bytes_per_address);
        } else {
          std::memcpy(out_ptr, &memory_[offset], bytes_per_address);
        }
      }

      if (success_mask && i < success_mask->size()) {
        (*success_mask)[i] = success ? 1 : 0;
      }
      if (!success) {
        all_success = false;
      }

      out_ptr += bytes_per_address;
    }

    if (success_mask) {
      return true;
    }

    return all_success;
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

  std::vector<mmem::ModuleDescriptor> GetModules() const override {
    return {};
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

  bool Suspend() override {
    return true;
  }

  bool Resume() override {
    return true;
  }

  void SetValid(bool valid) {
    is_valid_ = valid;
  }

 private:
  std::vector<std::byte> memory_;
  uintptr_t base_address_;
  bool is_valid_ = true;
  std::unordered_set<uintptr_t> invalid_addresses_;
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

  /// Waits for the async scan to complete and applies the result.
  void WaitForScan() {
    while (!model_.HasPendingResult()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    model_.ApplyPendingResult();
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
  WaitForScan();

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
  WaitForScan();

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
  WaitForScan();

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
  // Use aligned offsets (divisible by 4 for uint32_t)
  process_->WriteValue<uint32_t>(16, 100);
  process_->WriteValue<uint32_t>(32, 100);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(100));
  model_.FirstScan();
  WaitForScan();

  ASSERT_EQ(model_.entries().addresses.size(), 2);

  process_->WriteValue<uint32_t>(32, 101);

  model_.NextScan();

  const auto& entries = model_.entries();
  ASSERT_EQ(entries.addresses.size(), 1);
  EXPECT_EQ(entries.addresses[0], 0x100000 + 16);
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

  process_->WriteValue<uint32_t>(16, 999);  // Aligned offset

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(999));

  model_.FirstScan();
  WaitForScan();

  EXPECT_TRUE(listener.signal_received);
  EXPECT_EQ(listener.received_count, 1);
}

TEST_F(ScanResultModelTest, InvalidProcessDoesNothing) {
  process_->SetValid(false);
  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  // Wait a bit for the early exit path
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // No pending result for invalid process
  EXPECT_TRUE(model_.entries().addresses.empty());
}

TEST_F(ScanResultModelTest, ClearResetsStorage) {
  process_->WriteValue<uint32_t>(0, 123);
  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  WaitForScan();
  ASSERT_FALSE(model_.entries().addresses.empty());

  model_.Clear();

  EXPECT_TRUE(model_.entries().addresses.empty());
  EXPECT_TRUE(model_.entries().curr_raw.empty());
}

TEST_F(ScanResultModelTest, NextScanPopulatesPreviousValues) {
  process_->WriteValue<uint32_t>(100, 10);

  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  WaitForScan();

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
  EXPECT_EQ(prev_val, 20);
  EXPECT_EQ(curr_val, 20);
}

TEST_F(ScanResultModelTest, NextScanPreservesSnapshotAgainstAutoUpdate) {
  // Setup Initial State (Value = 10)
  // We use a specific address to ensure deterministic behavior.
  constexpr uintptr_t kAddressOffset = 0x10;
  process_->WriteValue<uint32_t>(kAddressOffset, 10);

  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  WaitForScan();

  // Sanity check: verify we found the initial value.
  ASSERT_FALSE(model_.entries().addresses.empty());

  // Change the value in RAM (Value = 20).
  process_->WriteValue<uint32_t>(kAddressOffset, 20);

  // TRIGGER THE BUG SCENARIO (Simulate Auto-Update)
  // We force the model to update its internal 'curr_raw' view to 20 *before*
  // the Next Scan occurs. This simulates the background thread waking up
  // and reading the new memory state while the user is looking at the UI.
  //
  // NOTE: This assumes UpdateCurrentValues is accessible (public or friend).
  // If private, use model_.StartAutoUpdate() and sleep_for(600ms).
  model_.UpdateCurrentValues();

  // Perform Next Scan (Looking for "Changed" values).
  model_.SetScanComparison(ScanComparison::kChanged);
  model_.NextScan();

  // Assertions
  // The entry should remain because 20 (Live) != 10 (Snapshot).
  // If this fails, it means NextScan used the 'dirty' auto-updated value
  // as the baseline instead of the 'clean' FirstScan value.
  EXPECT_FALSE(model_.entries().addresses.empty())
      << "Entry incorrectly removed! NextScan likely compared against the "
         "auto-updated value instead of the snapshot.";

  if (!model_.entries().addresses.empty()) {
    // Verify the model updated its values correctly after the successful scan
    const auto& entries = model_.entries();
    uint32_t current_val =
        *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
    uint32_t prev_val =
        *reinterpret_cast<const uint32_t*>(entries.prev_raw.data());

    EXPECT_EQ(current_val, 20) << "Current value should reflect RAM";
    EXPECT_EQ(prev_val, 20) << "Previous value should be updated to the new "
                               "baseline AFTER the scan succeeds";
  }
}

TEST_F(ScanResultModelTest,
       BugReproductionChangedFirstScanThenChangedNextScan) {
  // Setup Initial State (Value = 10)
  process_->WriteValue<uint32_t>(100, 10);

  // User selects "Changed" for the First Scan. This is technically "invalid"
  // for a first scan, but the UI might allow it, or the model should handle it
  // gracefully by treating it as Unknown for the *first* pass but remembering
  // "Changed" for the *next* pass.
  model_.SetScanComparison(ScanComparison::kChanged);

  // Execute First Scan
  model_.FirstScan();
  WaitForScan();

  // Verify First Scan behaved like "Unknown" (snapshot everything)
  ASSERT_FALSE(model_.entries().addresses.empty());

  // Change Value in RAM (Value = 20)
  process_->WriteValue<uint32_t>(100, 20);

  // Execute Next Scan (User expects "Changed" logic to persist). Crucially, we
  // do NOT call SetScanComparison again, simulating the user just clicking
  // "Next Scan".
  model_.NextScan();

  // Verify Result. Should find the address because 20 != 10. If bug exists,
  // this will be empty because model stuck in kUnknown.
  EXPECT_FALSE(model_.entries().addresses.empty());
}

TEST_F(ScanResultModelTest, NextScanIncreasedByFindsMatch) {
  process_->WriteValue<uint32_t>(100, 10);
  model_.SetScanComparison(ScanComparison::kUnknown);
  model_.FirstScan();
  WaitForScan();

  // Increase by 3 (10 -> 13)
  process_->WriteValue<uint32_t>(100, 13);

  model_.SetScanComparison(ScanComparison::kIncreasedBy);
  model_.SetTargetScanValue(ToBytes<uint32_t>(3));
  model_.NextScan();

  const auto& entries = model_.entries();
  ASSERT_FALSE(entries.addresses.empty());
  EXPECT_EQ(entries.addresses[0], 0x100000 + 100);

  // Verify value
  uint32_t val = *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
  EXPECT_EQ(val, 13);
}

TEST_F(ScanResultModelTest, NextScanGracefullyHandlesInvalidMemory) {
  // 1. Setup: First Scan finds 2 values
  process_->WriteValue<uint32_t>(100, 42);
  process_->WriteValue<uint32_t>(200, 42);

  model_.SetScanComparison(ScanComparison::kExactValue);
  model_.SetTargetScanValue(ToBytes<uint32_t>(42));
  model_.FirstScan();
  WaitForScan();

  ASSERT_EQ(model_.entries().addresses.size(), 2);

  // 2. Scenario: One address becomes invalid (e.g. unmapped page)
  process_->MarkAddressInvalid(0x100000 + 100);

  // 3. Action: Next Scan (Unchanged)
  model_.SetScanComparison(ScanComparison::kUnchanged);
  model_.NextScan();

  // 4. Expectation:
  // - Address 100 is REMOVED (because it's invalid)
  // - Address 200 is KEPT (because it's valid and unchanged)
  // The scan should NOT abort.

  const auto& entries = model_.entries();
  ASSERT_EQ(entries.addresses.size(), 1);
  EXPECT_EQ(entries.addresses[0], 0x100000 + 200);
}

}  // namespace

}  // namespace maia
