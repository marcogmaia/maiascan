// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "maia/tests/fake_process.h"

namespace maia {
namespace {

class ScanResultModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    process_ = std::make_unique<test::FakeProcess>(1024);
    model_ = std::make_unique<ScanResultModel>();
    model_->SetActiveProcess(process_.get());
    model_->StopAutoUpdate();
  }

  void TearDown() override {
    if (model_) {
      model_->Clear();
    }
  }

  void WaitForScanComplete() {
    model_->WaitForScanToFinish();
    model_->ApplyPendingResult();
  }

  void Scan() {
    model_->FirstScan();
    WaitForScanComplete();
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

class ScanResultModelLogicTest : public ScanResultModelTest {
 protected:
  void SetUp() override {
    model_ = std::make_unique<ScanResultModel>(4096);
    process_ = std::make_unique<test::FakeProcess>(8192);
    model_->SetActiveProcess(process_.get());
    model_->StopAutoUpdate();
  }
};

class ScanResultModelChunkedTest : public ScanResultModelTest {
 protected:
  void SetUp() override {
    process_ = std::make_unique<test::FakeProcess>(40 * 1024 * 1024);
    model_ = std::make_unique<ScanResultModel>();
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

  Scan();

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

  Scan();

  const auto& entries = model_->entries();
  EXPECT_GT(entries.addresses.size(), 250);

  uint32_t val0 = *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
  EXPECT_EQ(val0, 10);
}

TEST_F(ScanResultModelTest, NextScanIncreasedValueFiltersResults) {
  process_->WriteValue<uint32_t>(100, 10);
  process_->WriteValue<uint32_t>(200, 50);

  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();

  process_->WriteValue<uint32_t>(100, 15);

  model_->SetScanComparison(ScanComparison::kIncreased);
  model_->NextScan();
  WaitForScanComplete();

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
  process_->WriteValue<uint32_t>(16, 100);
  process_->WriteValue<uint32_t>(32, 100);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(100));
  Scan();

  ASSERT_EQ(model_->entries().addresses.size(), 2);

  process_->WriteValue<uint32_t>(32, 101);

  model_->NextScan();
  WaitForScanComplete();

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

  process_->WriteValue<uint32_t>(16, 999);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(999));

  Scan();

  EXPECT_TRUE(listener.signal_received);
  EXPECT_EQ(listener.received_count, 1);
}

TEST_F(ScanResultModelTest, InvalidProcessDoesNothing) {
  process_->SetValid(false);
  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  EXPECT_FALSE(model_->IsScanning());
  EXPECT_TRUE(model_->entries().addresses.empty());
}

TEST_F(ScanResultModelTest, ClearResetsStorage) {
  process_->WriteValue<uint32_t>(0, 123);
  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();
  ASSERT_FALSE(model_->entries().addresses.empty());

  model_->Clear();

  EXPECT_TRUE(model_->entries().addresses.empty());
  EXPECT_TRUE(model_->entries().curr_raw.empty());
}

TEST_F(ScanResultModelTest, NextScanPopulatesPreviousValues) {
  process_->WriteValue<uint32_t>(100, 10);

  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();

  process_->WriteValue<uint32_t>(100, 20);
  model_->SetScanComparison(ScanComparison::kChanged);
  model_->NextScan();
  WaitForScanComplete();

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
  constexpr uintptr_t kAddressOffset = 0x10;
  process_->WriteValue<uint32_t>(kAddressOffset, 10);

  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();

  ASSERT_FALSE(model_->entries().addresses.empty());

  process_->WriteValue<uint32_t>(kAddressOffset, 20);

  model_->UpdateCurrentValues();

  model_->SetScanComparison(ScanComparison::kChanged);
  model_->NextScan();
  WaitForScanComplete();

  EXPECT_FALSE(model_->entries().addresses.empty())
      << "Entry incorrectly removed! NextScan likely compared against the "
         "auto-updated value instead of the snapshot.";

  if (!model_->entries().addresses.empty()) {
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
  process_->WriteValue<uint32_t>(100, 10);

  model_->SetScanComparison(ScanComparison::kChanged);

  Scan();

  ASSERT_FALSE(model_->entries().addresses.empty());

  process_->WriteValue<uint32_t>(100, 20);

  model_->NextScan();
  WaitForScanComplete();

  EXPECT_FALSE(model_->entries().addresses.empty());
}

TEST_F(ScanResultModelTest, NextScanIncreasedByFindsMatch) {
  process_->WriteValue<uint32_t>(100, 10);
  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();

  process_->WriteValue<uint32_t>(100, 13);

  model_->SetScanComparison(ScanComparison::kIncreasedBy);
  model_->SetTargetScanValue(ToBytes<uint32_t>(3));
  model_->NextScan();
  WaitForScanComplete();

  const auto& entries = model_->entries();
  ASSERT_FALSE(entries.addresses.empty());
  EXPECT_EQ(entries.addresses[0], 0x100000 + 100);

  uint32_t val = *reinterpret_cast<const uint32_t*>(entries.curr_raw.data());
  EXPECT_EQ(val, 13);
}

TEST_F(ScanResultModelTest, NextScanGracefullyHandlesInvalidMemory) {
  process_->WriteValue<uint32_t>(100, 42);
  process_->WriteValue<uint32_t>(200, 42);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(42));
  Scan();

  ASSERT_EQ(model_->entries().addresses.size(), 2);

  process_->MarkAddressInvalid(0x100000 + 100);

  model_->SetScanComparison(ScanComparison::kUnchanged);
  model_->NextScan();
  WaitForScanComplete();

  const auto& entries = model_->entries();
  ASSERT_EQ(entries.addresses.size(), 1);
  EXPECT_EQ(entries.addresses[0], 0x100000 + 200);
}

TEST_F(ScanResultModelTest, FirstScanAobFindsMatches) {
  uint8_t pattern[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  std::memcpy(process_->GetRawMemory().data() + 300, pattern, 5);

  uint8_t noisy[] = {0xAA, 0xBB, 0x00, 0xDD, 0xEE};
  std::memcpy(process_->GetRawMemory().data() + 600, noisy, 5);

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

  Scan();

  const auto& storage = model_->entries();
  EXPECT_EQ(storage.stride, 5);
}

TEST_F(ScanResultModelTest, FirstScanStringWithSpacesFindsMatches) {
  const char* text = "hello world";
  std::memcpy(process_->GetRawMemory().data() + 400, text, std::strlen(text));

  std::string search = "hello wor";
  std::vector<std::byte> val;
  for (char c : search) {
    val.push_back(static_cast<std::byte>(c));
  }

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kString);
  model_->SetTargetScanValue(val);

  Scan();

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

  std::string search = "hello wor";
  std::vector<std::byte> val;
  for (char c : search) {
    val.push_back(static_cast<std::byte>(c));
  }

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kString);
  model_->SetTargetScanValue(val);

  Scan();

  const auto& storage = model_->entries();
  ASSERT_EQ(storage.addresses.size(), 1);
  EXPECT_EQ(storage.addresses[0], 0x100000 + 401);
}

// --- Logic Tests ---

TEST_F(ScanResultModelLogicTest,
       UnknownScanFindsUnalignedWhenFastScanDisabled) {
  model_->SetFastScan(false);
  model_->SetScanComparison(ScanComparison::kUnknown);

  Scan();

  const auto& storage = model_->entries();
  const uintptr_t base = process_->GetBaseAddress();

  ASSERT_GT(storage.addresses.size(), 8000);
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + 1);
  EXPECT_EQ(storage.addresses[2], base + 2);
  EXPECT_EQ(storage.addresses[3], base + 3);

  EXPECT_EQ(storage.stride, sizeof(uint32_t));
}

TEST_F(ScanResultModelLogicTest, UnknownScanSnapshotsAcrossChunks) {
  model_->SetScanComparison(ScanComparison::kUnknown);
  Scan();

  const auto& storage = model_->entries();
  const uintptr_t base = process_->GetBaseAddress();

  ASSERT_EQ(storage.addresses.size(), 2048);

  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1023], base + 4092);

  EXPECT_EQ(storage.addresses[1024], base + 4096);
  EXPECT_EQ(storage.addresses[2047], base + 8188);

  for (size_t i = 0; i < storage.addresses.size(); ++i) {
    EXPECT_EQ(storage.addresses[i], base + (i * 4))
        << "Gap found at index " << i;
  }
}

// --- Chunked Tests ---

TEST_F(ScanResultModelChunkedTest, FindsMatchCrossingChunkBoundary) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;

  const size_t near_boundary_offset = kChunkSize - 4;
  const uint32_t magic_value = 0xDEADBEEF;

  process_->WriteValue<uint32_t>(near_boundary_offset, magic_value);

  process_->WriteValue<uint32_t>(100, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  Scan();

  const auto& storage = model_->entries();

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
  const uint32_t magic_value = 0xCAFEBABE;

  process_->WriteValue<uint32_t>(0, magic_value);
  process_->WriteValue<uint32_t>(100, magic_value);
  process_->WriteValue<uint32_t>(1000, magic_value);

  process_->WriteValue<uint32_t>(201, magic_value);
  process_->WriteValue<uint32_t>(307, magic_value);
  process_->WriteValue<uint32_t>(503, magic_value);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  Scan();

  const auto& storage = model_->entries();
  uintptr_t base = process_->GetBaseAddress();

  ASSERT_EQ(storage.addresses.size(), 3)
      << "Should only find aligned matches, not unaligned ones";

  for (auto addr : storage.addresses) {
    size_t offset = addr - base;
    EXPECT_EQ(offset % 4, 0) << "Found unaligned address at offset " << offset;
  }

  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + 100);
  EXPECT_EQ(storage.addresses[2], base + 1000);
}

TEST_F(ScanResultModelChunkedTest, ExactScanUnalignedOnlyFindsNothing) {
  const uint32_t magic_value = 0xDEADC0DE;

  process_->WriteValue<uint32_t>(101, magic_value);
  process_->WriteValue<uint32_t>(205, magic_value);
  process_->WriteValue<uint32_t>(309, magic_value);
  process_->WriteValue<uint32_t>(413, magic_value);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  Scan();

  const auto& storage = model_->entries();

  EXPECT_EQ(storage.addresses.size(), 0)
      << "Should not find any matches when all are unaligned";
}

TEST_F(ScanResultModelChunkedTest, AlignmentAcrossChunkBoundary) {
  constexpr size_t kChunkSize = 32 * 1024 * 1024;
  const uint32_t magic_value = 0xBEEFCAFE;

  process_->WriteValue<uint32_t>(0, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  process_->WriteValue<uint32_t>(kChunkSize + 201, magic_value);
  process_->WriteValue<uint32_t>(kChunkSize + 303, magic_value);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  Scan();

  const auto& storage = model_->entries();
  uintptr_t base = process_->GetBaseAddress();

  ASSERT_EQ(storage.addresses.size(), 3);
  EXPECT_EQ(storage.addresses[0], base + 0);
  EXPECT_EQ(storage.addresses[1], base + kChunkSize);
  EXPECT_EQ(storage.addresses[2], base + kChunkSize + 100);
}

TEST_F(ScanResultModelChunkedTest, FindsUnalignedWhenFastScanDisabled) {
  const uint32_t magic_value = 0xCAFEBABE;
  const uintptr_t base = process_->GetBaseAddress();

  process_->WriteValue<uint32_t>(1, magic_value);
  process_->WriteValue<uint32_t>(13, magic_value);

  model_->SetFastScan(false);
  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(magic_value));

  Scan();

  const auto& storage = model_->entries();

  ASSERT_EQ(storage.addresses.size(), 2);
  EXPECT_EQ(storage.addresses[0], base + 1);
  EXPECT_EQ(storage.addresses[1], base + 13);
}

TEST_F(ScanResultModelChunkedTest, DestructorDoesNotHangWhenScanning) {
  model_->SetScanComparison(ScanComparison::kUnknown);
  model_->FirstScan();
  model_.reset();
}

TEST_F(ScanResultModelTest, CommittedConfigMatchesScanTimeSettings) {
  process_->WriteValue<uint32_t>(100, 42);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(42));
  model_->FirstScan();

  model_->SetTargetScanValue(ToBytes<uint32_t>(99));

  WaitForScanComplete();

  core::ScanConfig committed_config = model_->GetSessionConfig();

  ASSERT_EQ(committed_config.value.size(), sizeof(uint32_t));
  uint32_t committed_value =
      *reinterpret_cast<const uint32_t*>(committed_config.value.data());
  EXPECT_EQ(committed_value, 42)
      << "Committed config should use the value from scan start (42), not the "
         "value changed mid-scan (99)";
}

}  // namespace
}  // namespace maia
