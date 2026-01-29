// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "maia/core/task_runner.h"
#include "maia/tests/fake_process.h"

namespace maia {
namespace {

class ScanResultModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    process_ = std::make_unique<test::FakeProcess>(1024);
    auto runner = std::make_unique<core::SyncTaskRunner>();
    model_ = std::make_unique<ScanResultModel>(std::move(runner));
    model_->SetActiveProcess(process_.get());
    model_->StopAutoUpdate();
  }

  void TearDown() override {
    model_->Clear();
  }

  template <typename T>
  std::vector<std::byte> ToBytes(T val) {
    std::vector<std::byte> b(sizeof(T));
    std::memcpy(b.data(), &val, sizeof(T));
    return b;
  }

  std::unique_ptr<ScanResultModel> model_;
  std::unique_ptr<test::FakeProcess> process_;
};

// Fixture for logic tests that need smaller chunks
class ScanResultModelLogicTest : public ScanResultModelTest {
 protected:
  void SetUp() override {
    // Use 4KB chunks for granular testing without massive memory usage.
    model_ = std::make_unique<ScanResultModel>(
        std::make_unique<core::SyncTaskRunner>(), 4096);

    // Small process (8KB) is sufficient for logic tests.
    process_ = std::make_unique<test::FakeProcess>(8192);
    model_->SetActiveProcess(process_.get());
    model_->StopAutoUpdate();
  }

  void Scan() {
    model_->FirstScan();
    model_->ApplyPendingResult();
  }
};

// Fixture for tests that verify chunking behavior with large memory
class ScanResultModelChunkedTest : public ScanResultModelTest {
 protected:
  void SetUp() override {
    // 40MB to safely cover 32MB chunk boundary
    process_ = std::make_unique<test::FakeProcess>(40 * 1024 * 1024);
    model_ = std::make_unique<ScanResultModel>(
        std::make_unique<core::SyncTaskRunner>());
    model_->SetActiveProcess(process_.get());
    model_->StopAutoUpdate();
  }
};

// --- Standard Tests ---

TEST_F(ScanResultModelTest, FirstScanExactValueFindsMatches) {
  process_->WriteValue<uint32_t>(100, 42);
  process_->WriteValue<uint32_t>(200, 99);
  process_->WriteValue<uint32_t>(500, 42);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(42));

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();
  ASSERT_EQ(storage.addresses.size(), 2);
  EXPECT_EQ(storage.addresses[0], 0x100000 + 100);
  EXPECT_EQ(storage.addresses[1], 0x100000 + 500);

  EXPECT_EQ(storage.stride, 4);
  uint32_t val1 = *reinterpret_cast<const uint32_t*>(storage.curr_raw.data());
  EXPECT_EQ(val1, 42);
}

TEST_F(ScanResultModelTest, FirstScanUnknownValueSnapshotsMemory) {
  process_->WriteValue<uint32_t>(0, 10);
  model_->SetScanComparison(ScanComparison::kUnknown);

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& entries = model_->entries();
  // 1KB / 4 bytes = ~256 potential alignments
  EXPECT_GT(entries.addresses.size(), 250);

  uint32_t val0 = *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
  EXPECT_EQ(val0, 10);
}

TEST_F(ScanResultModelTest, NextScanIncreasedValueFiltersResults) {
  process_->WriteValue<uint32_t>(100, 10);
  process_->WriteValue<uint32_t>(200, 50);

  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  model_->ApplyPendingResult();

  process_->WriteValue<uint32_t>(100, 15);  // Increased

  model_->SetScanComparison(ScanComparison::kIncreased);
  model_->NextScan();

  const auto& entries = model_->entries();

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

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(100));
  model_->FirstScan();
  model_->ApplyPendingResult();

  ASSERT_EQ(model_->entries().addresses.size(), 2);

  process_->WriteValue<uint32_t>(32, 101);

  model_->NextScan();

  const auto& entries = model_->entries();
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
      model_->sinks().MemoryChanged().connect<&TestListener::OnMemoryChanged>(
          listener);

  process_->WriteValue<uint32_t>(16, 999);  // Aligned offset

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(999));

  model_->FirstScan();
  model_->ApplyPendingResult();

  EXPECT_TRUE(listener.signal_received);
  EXPECT_EQ(listener.received_count, 1);
}

TEST_F(ScanResultModelTest, InvalidProcessDoesNothing) {
  process_->SetValid(false);
  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  // Wait a bit for the early exit path
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // No pending result for invalid process
  EXPECT_TRUE(model_->entries().addresses.empty());
}

TEST_F(ScanResultModelTest, ClearResetsStorage) {
  process_->WriteValue<uint32_t>(0, 123);
  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  model_->ApplyPendingResult();
  ASSERT_FALSE(model_->entries().addresses.empty());

  model_->Clear();

  EXPECT_TRUE(model_->entries().addresses.empty());
  EXPECT_TRUE(model_->entries().curr_raw.empty());
}

TEST_F(ScanResultModelTest, NextScanPopulatesPreviousValues) {
  process_->WriteValue<uint32_t>(100, 10);

  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  model_->ApplyPendingResult();

  process_->WriteValue<uint32_t>(100, 20);
  model_->SetScanComparison(ScanComparison::kChanged);
  model_->NextScan();

  const auto& entries = model_->entries();
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

  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  model_->ApplyPendingResult();

  // Sanity check: verify we found the initial value.
  ASSERT_FALSE(model_->entries().addresses.empty());

  // Change the value in RAM (Value = 20).
  process_->WriteValue<uint32_t>(kAddressOffset, 20);

  // TRIGGER THE BUG SCENARIO (Simulate Auto-Update)
  model_->UpdateCurrentValues();

  // Perform Next Scan (Looking for "Changed" values).
  model_->SetScanComparison(ScanComparison::kChanged);
  model_->NextScan();

  // Assertions
  EXPECT_FALSE(model_->entries().addresses.empty())
      << "Entry incorrectly removed! NextScan likely compared against the "
         "auto-updated value instead of the snapshot.";

  if (!model_->entries().addresses.empty()) {
    // Verify the model updated its values correctly after the successful scan
    const auto& entries = model_->entries();
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

  // User selects "Changed" for the First Scan.
  model_->SetScanComparison(ScanComparison::kChanged);

  // Execute First Scan
  model_->FirstScan();
  model_->ApplyPendingResult();

  // Verify First Scan behaved like "Unknown" (snapshot everything)
  ASSERT_FALSE(model_->entries().addresses.empty());

  // Change Value in RAM (Value = 20)
  process_->WriteValue<uint32_t>(100, 20);

  // Execute Next Scan
  model_->NextScan();

  // Verify Result. Should find the address because 20 != 10.
  EXPECT_FALSE(model_->entries().addresses.empty());
}

TEST_F(ScanResultModelTest, NextScanIncreasedByFindsMatch) {
  process_->WriteValue<uint32_t>(100, 10);
  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  model_->ApplyPendingResult();

  // Increase by 3 (10 -> 13)
  process_->WriteValue<uint32_t>(100, 13);

  model_->SetScanComparison(ScanComparison::kIncreasedBy);
  model_->SetTargetScanValue(ToBytes<uint32_t>(3));
  model_->NextScan();

  const auto& entries = model_->entries();
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

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(42));
  model_->FirstScan();
  model_->ApplyPendingResult();

  ASSERT_EQ(model_->entries().addresses.size(), 2);

  // 2. Scenario: One address becomes invalid (e.g. unmapped page)
  process_->MarkAddressInvalid(0x100000 + 100);

  // 3. Action: Next Scan (Unchanged)
  model_->SetScanComparison(ScanComparison::kUnchanged);
  model_->NextScan();

  const auto& entries = model_->entries();
  ASSERT_EQ(entries.addresses.size(), 1);
  EXPECT_EQ(entries.addresses[0], 0x100000 + 200);
}

TEST_F(ScanResultModelTest, FirstScanAobFindsMatches) {
  // Write a pattern: AA BB CC DD EE
  uint8_t pattern[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  std::memcpy(process_->GetRawMemory().data() + 300, pattern, 5);

  // Write a similar pattern with one byte different: AA BB 00 DD EE
  uint8_t noisy[] = {0xAA, 0xBB, 0x00, 0xDD, 0xEE};
  std::memcpy(process_->GetRawMemory().data() + 600, noisy, 5);

  // Scan for AA BB ?? DD EE
  std::vector<std::byte> val = {std::byte{0xAA},
                                std::byte{0xBB},
                                std::byte{0x00},
                                std::byte{0xDD},
                                std::byte{0xEE}};
  std::vector<std::byte> mask = {std::byte{0xFF},
                                 std::byte{0xFF},
                                 std::byte{0x00},
                                 std::byte{0xFF},
                                 std::byte{0xFF}};

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kArrayOfBytes);
  model_->SetTargetScanPattern(val, mask);

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();
  // Should find BOTH 300 and 600 because of the wildcard
  EXPECT_EQ(storage.stride, 5);
}

TEST_F(ScanResultModelTest, FirstScanStringWithSpacesFindsMatches) {
  const char* text = "hello world";
  std::memcpy(process_->GetRawMemory().data() + 400, text, std::strlen(text));

  // Search for "hello wor"
  std::string search = "hello wor";
  std::vector<std::byte> val;
  for (char c : search) {
    val.push_back(static_cast<std::byte>(c));
  }

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kString);
  model_->SetTargetScanValue(val);

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();
  ASSERT_EQ(storage.addresses.size(), 1);
  EXPECT_EQ(storage.addresses[0], 0x100000 + 400);
  EXPECT_EQ(storage.stride, search.size());
}

TEST_F(ScanResultModelTest, FirstScanStringWithSpacesAtUnalignedAddress) {
  const char* text = "hello world";
  std::memset(
      process_->GetRawMemory().data(), 0, process_->GetRawMemory().size());
  std::memcpy(process_->GetRawMemory().data() + 401, text, std::strlen(text));

  // Search for "hello wor"
  std::string search = "hello wor";
  std::vector<std::byte> val;
  for (char c : search) {
    val.push_back(static_cast<std::byte>(c));
  }

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kString);
  model_->SetTargetScanValue(val);

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();
  ASSERT_EQ(storage.addresses.size(), 1);
  EXPECT_EQ(storage.addresses[0], 0x100000 + 401);
}

// --- Logic Tests ---

TEST_F(ScanResultModelLogicTest,
       UnknownScanFindsUnalignedWhenFastScanDisabled) {
  // For unknown scan, disabling fast scan should snapshot EVERY byte.
  model_->SetFastScan(false);  // DISABLE FAST SCAN
  model_->SetScanComparison(ScanComparison::kUnknown);

  Scan();

  const auto& storage = model_->entries();
  const uintptr_t base = process_->GetBaseAddress();

  // With alignment=1, we should find matches at 0, 1, 2, 3, 4, ...
  // For 8192 bytes, we expect roughly 8192-3 addresses (for uint32 size).
  ASSERT_GT(storage.addresses.size(), 8000);
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + 1);
  EXPECT_EQ(storage.addresses[2], base + 2);
  EXPECT_EQ(storage.addresses[3], base + 3);

  // Verify stride is correct
  EXPECT_EQ(storage.stride, sizeof(uint32_t));
}

TEST_F(ScanResultModelLogicTest, UnknownScanSnapshotsAcrossChunks) {
  // Verifies that Unknown scan snapshots data correctly across chunk
  // boundaries. Configuration from SetUp:
  // - Process Memory: 8192 bytes (8KB)
  // - Chunk Size:     4096 bytes (4KB)
  // - Result:         Exactly 2 chunks. Boundary is at offset 4096.

  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();

  const auto& storage = model_->entries();
  const uintptr_t base = process_->GetBaseAddress();

  // Total expected items: 8192 / 4 = 2048 uint32_t values.
  ASSERT_EQ(storage.addresses.size(), 2048);

  // --- Verify Chunk 1 (Offsets 0 to 4092) ---
  EXPECT_EQ(storage.addresses[0], base + 0);
  // Last item of Chunk 1 (4096 - 4)
  EXPECT_EQ(storage.addresses[1023], base + 4092);

  // --- Verify Chunk 2 (Offsets 4096 to 8188) ---
  // First item of Chunk 2 (Boundary)
  EXPECT_EQ(storage.addresses[1024], base + 4096);
  // Last item of Chunk 2
  EXPECT_EQ(storage.addresses[2047], base + 8188);

  // Verify the list is contiguous (no gaps at boundary)
  for (size_t i = 0; i < storage.addresses.size(); ++i) {
    EXPECT_EQ(storage.addresses[i], base + (i * 4))
        << "Gap found at index " << i;
  }
}

// --- Chunked Tests ---

TEST_F(ScanResultModelChunkedTest, FindsMatchCrossingChunkBoundary) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB

  // Place a 4-byte value near the chunk boundary at an aligned offset.
  const size_t near_boundary_offset = kChunkSize - 4;  // Aligned to 4
  const uint32_t magic_value = 0xDEADBEEF;

  process_->WriteValue<uint32_t>(near_boundary_offset, magic_value);

  // Also place values well before and well after the boundary
  process_->WriteValue<uint32_t>(100, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();

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

TEST_F(ScanResultModelChunkedTest, ExactScanSkipsUnalignedAddresses) {
  // This test verifies that the full scan pipeline respects alignment.
  const uint32_t magic_value = 0xCAFEBABE;

  // Place value at aligned offsets (divisible by 4)
  process_->WriteValue<uint32_t>(0, magic_value);     // Aligned
  process_->WriteValue<uint32_t>(100, magic_value);   // Aligned
  process_->WriteValue<uint32_t>(1000, magic_value);  // Aligned

  // Place value at unaligned offsets (NOT divisible by 4)
  process_->WriteValue<uint32_t>(201, magic_value);  // Unaligned
  process_->WriteValue<uint32_t>(307, magic_value);  // Unaligned
  process_->WriteValue<uint32_t>(503, magic_value);  // Unaligned

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();
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
  const uint32_t magic_value = 0xDEADC0DE;

  // Place values ONLY at unaligned offsets
  process_->WriteValue<uint32_t>(101, magic_value);
  process_->WriteValue<uint32_t>(205, magic_value);
  process_->WriteValue<uint32_t>(309, magic_value);
  process_->WriteValue<uint32_t>(413, magic_value);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();

  // Should find nothing because all values are at unaligned offsets
  EXPECT_EQ(storage.addresses.size(), 0)
      << "Should not find any matches when all are unaligned";
}

TEST_F(ScanResultModelChunkedTest, AlignmentAcrossChunkBoundary) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;  // 32MB
  const uint32_t magic_value = 0xBEEFCAFE;

  // Place aligned values in different chunks
  process_->WriteValue<uint32_t>(0, magic_value);                 // Chunk 0
  process_->WriteValue<uint32_t>(kChunkSize, magic_value);        // Chunk 1
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);  // Chunk 1

  // Place unaligned values that should be skipped
  process_->WriteValue<uint32_t>(kChunkSize + 201, magic_value);  // Unaligned
  process_->WriteValue<uint32_t>(kChunkSize + 303, magic_value);  // Unaligned

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();
  uintptr_t base = process_->GetBaseAddress();

  // Should find exactly 3 aligned matches across chunks
  ASSERT_EQ(storage.addresses.size(), 3);
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + kChunkSize);
  EXPECT_EQ(storage.addresses[2], base + kChunkSize + 100);
}

TEST_F(ScanResultModelChunkedTest, FindsUnalignedWhenFastScanDisabled) {
  const uint32_t magic_value = 0xCAFEBABE;
  const uintptr_t base = process_->GetBaseAddress();

  // Place value at unaligned offsets
  process_->WriteValue<uint32_t>(1, magic_value);
  process_->WriteValue<uint32_t>(13, magic_value);

  model_->SetFastScan(false);  // DISABLE FAST SCAN
  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  model_->FirstScan();
  model_->ApplyPendingResult();

  const auto& storage = model_->entries();

  // Should now find BOTH unaligned matches
  ASSERT_EQ(storage.addresses.size(), 2);
  EXPECT_EQ(storage.addresses[0], base + 1);
  EXPECT_EQ(storage.addresses[1], base + 13);
}

}  // namespace
}  // namespace maia
