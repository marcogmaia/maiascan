// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <vector>

#include "maia/tests/fake_process.h"

namespace maia {
namespace {

class ScanResultModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Init(1024, static_cast<size_t>(32 * 1024 * 1024));
  }

  void TearDown() override {
    if (model_) {
      model_->Clear();
    }
  }

  void Init(size_t process_size, size_t chunk_size) {
    process_ = std::make_unique<test::FakeProcess>(process_size);
    model_ = std::make_unique<ScanResultModel>(chunk_size);
    model_->SetActiveProcess(process_.get());
    model_->StopAutoUpdate();
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

  static std::vector<std::byte> StringToBytes(std::string_view str) {
    std::vector<std::byte> result;
    for (char c : str) {
      result.push_back(static_cast<std::byte>(c));
    }
    return result;
  }

  template <typename T>
  void WriteValue(size_t offset, T value) {
    process_->WriteValue<T>(offset, value);
  }

  template <typename T>
  void PerformFirstScan(ScanComparison comparison,
                        std::optional<T> value = std::nullopt) {
    model_->SetScanComparison(comparison);
    if (value.has_value()) {
      model_->SetTargetScanValue(ToBytes(*value));
    }
    Scan();
  }

  template <typename T>
  void PerformNextScan(ScanComparison comparison,
                       std::optional<T> value = std::nullopt) {
    model_->SetScanComparison(comparison);
    if (value.has_value()) {
      model_->SetTargetScanValue(ToBytes(*value));
    }
    model_->NextScan();
    WaitForScanComplete();
  }

  void StartScanWithoutWaiting(ScanComparison comparison) {
    model_->SetScanComparison(comparison);
    model_->FirstScan();
  }

  void VerifyAddressCount(size_t expected) {
    EXPECT_EQ(model_->entries().addresses.size(), expected);
  }

  void VerifyAddresses(const std::vector<size_t>& expected_offsets) {
    const auto& addresses = model_->entries().addresses;
    ASSERT_EQ(addresses.size(), expected_offsets.size());
    uintptr_t base = process_->GetBaseAddress();
    for (size_t i = 0; i < expected_offsets.size(); ++i) {
      EXPECT_EQ(addresses[i], base + expected_offsets[i]);
    }
  }

  template <typename T>
  void VerifyFirstValue(T expected) {
    ASSERT_GE(model_->entries().curr_raw.size(), sizeof(T));
    T actual = *reinterpret_cast<const T*>(model_->entries().curr_raw.data());
    EXPECT_EQ(actual, expected);
  }

  template <typename T>
  void VerifyPrevValue(T expected) {
    ASSERT_GE(model_->entries().prev_raw.size(), sizeof(T));
    T actual = *reinterpret_cast<const T*>(model_->entries().prev_raw.data());
    EXPECT_EQ(actual, expected);
  }

  void VerifyStride(size_t expected) {
    EXPECT_EQ(model_->entries().stride, expected);
  }

  template <typename T>
  T GetCommittedValue() {
    core::ScanConfig config = model_->GetSessionConfig();
    EXPECT_EQ(config.value.size(), sizeof(T));
    return *reinterpret_cast<const T*>(config.value.data());
  }

  std::unique_ptr<ScanResultModel> model_;
  std::unique_ptr<test::FakeProcess> process_;
};

class ScanResultModelLogicTest : public ScanResultModelTest {
 protected:
  void SetUp() override {
    Init(8192, 4096);
  }
};

class ScanResultModelChunkedTest : public ScanResultModelTest {
 protected:
  void SetUp() override {
    Init(static_cast<size_t>(40 * 1024 * 1024),
         static_cast<size_t>(32 * 1024 * 1024));
  }
};

// --- Standard Tests ---

TEST_F(ScanResultModelTest, FirstScanExactValueFindsMatches) {
  WriteValue<uint32_t>(100, 42);
  WriteValue<uint32_t>(200, 99);
  WriteValue<uint32_t>(500, 42);

  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, 42);

  VerifyAddressCount(2);
  VerifyAddresses({100, 500});
  VerifyStride(sizeof(uint32_t));
  VerifyFirstValue<uint32_t>(42);
}

TEST_F(ScanResultModelTest, FirstScanUnknownValueSnapshotsMemory) {
  WriteValue<uint32_t>(0, 10);

  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);

  EXPECT_GT(model_->entries().addresses.size(), 250);
  VerifyFirstValue<uint32_t>(10);
}

TEST_F(ScanResultModelTest, NextScanIncreasedValueFiltersResults) {
  WriteValue<uint32_t>(100, 10);
  WriteValue<uint32_t>(200, 50);

  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);

  WriteValue<uint32_t>(100, 15);

  PerformNextScan<uint32_t>(ScanComparison::kIncreased);

  VerifyAddressCount(1);
  VerifyAddresses({100});
}

TEST_F(ScanResultModelTest, NextScanExactValueFiltersResults) {
  WriteValue<uint32_t>(16, 100);
  WriteValue<uint32_t>(32, 100);

  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, 100);
  VerifyAddressCount(2);

  WriteValue<uint32_t>(32, 101);

  PerformNextScan<uint32_t>(ScanComparison::kExactValue, 100);

  VerifyAddressCount(1);
  VerifyAddresses({16});
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

  WriteValue<uint32_t>(16, 999);
  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, 999);

  EXPECT_TRUE(listener.signal_received);
  EXPECT_EQ(listener.received_count, 1);
}

TEST_F(ScanResultModelTest, InvalidProcessDoesNothing) {
  process_->SetValid(false);
  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);
  EXPECT_FALSE(model_->IsScanning());
  EXPECT_TRUE(model_->entries().addresses.empty());
}

TEST_F(ScanResultModelTest, ClearResetsStorage) {
  WriteValue<uint32_t>(0, 123);
  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);
  ASSERT_FALSE(model_->entries().addresses.empty());

  model_->Clear();

  EXPECT_TRUE(model_->entries().addresses.empty());
  EXPECT_TRUE(model_->entries().curr_raw.empty());
}

TEST_F(ScanResultModelTest, NextScanPopulatesPreviousValues) {
  WriteValue<uint32_t>(100, 10);

  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);

  WriteValue<uint32_t>(100, 20);
  PerformNextScan<uint32_t>(ScanComparison::kChanged);

  VerifyAddressCount(1);
  VerifyFirstValue<uint32_t>(20);
  VerifyPrevValue<uint32_t>(20);
}

TEST_F(ScanResultModelTest, NextScanPreservesSnapshotAgainstAutoUpdate) {
  constexpr size_t kAddressOffset = 0x10;
  WriteValue<uint32_t>(kAddressOffset, 10);

  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);
  ASSERT_FALSE(model_->entries().addresses.empty());

  WriteValue<uint32_t>(kAddressOffset, 20);
  model_->UpdateCurrentValues();

  PerformNextScan<uint32_t>(ScanComparison::kChanged);

  VerifyAddressCount(1);
  VerifyFirstValue<uint32_t>(20);
  VerifyPrevValue<uint32_t>(20);
}

TEST_F(ScanResultModelTest,
       BugReproductionChangedFirstScanThenChangedNextScan) {
  WriteValue<uint32_t>(100, 10);

  PerformFirstScan<uint32_t>(ScanComparison::kChanged);
  ASSERT_FALSE(model_->entries().addresses.empty());

  WriteValue<uint32_t>(100, 20);
  PerformNextScan<uint32_t>(ScanComparison::kChanged);

  VerifyAddressCount(1);
}

TEST_F(ScanResultModelTest, NextScanIncreasedByFindsMatch) {
  WriteValue<uint32_t>(100, 10);
  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);

  WriteValue<uint32_t>(100, 13);

  PerformNextScan<uint32_t>(ScanComparison::kIncreasedBy, 3);

  VerifyAddressCount(1);
  VerifyAddresses({100});
  VerifyFirstValue<uint32_t>(13);
}

TEST_F(ScanResultModelTest, NextScanGracefullyHandlesInvalidMemory) {
  WriteValue<uint32_t>(100, 42);
  WriteValue<uint32_t>(200, 42);

  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, 42);
  VerifyAddressCount(2);

  process_->MarkAddressInvalid(process_->GetBaseAddress() + 100);

  PerformNextScan<uint32_t>(ScanComparison::kUnchanged);

  VerifyAddressCount(1);
  VerifyAddresses({200});
}

TEST_F(ScanResultModelTest, FirstScanAobFindsMatches) {
  uint8_t pattern[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  std::memcpy(process_->GetRawMemory().data() + 300, pattern, 5);

  uint8_t noisy[] = {0xAA, 0xBB, 0x00, 0xDD, 0xEE};
  std::memcpy(process_->GetRawMemory().data() + 600, noisy, 5);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kArrayOfBytes);
  model_->SetTargetScanPattern({std::byte{0xAA},
                                std::byte{0xBB},
                                std::byte{0x00},
                                std::byte{0xDD},
                                std::byte{0xEE}},
                               {std::byte{0xFF},
                                std::byte{0xFF},
                                std::byte{0x00},
                                std::byte{0xFF},
                                std::byte{0xFF}});

  Scan();

  VerifyStride(5);
}

TEST_F(ScanResultModelTest, FirstScanStringWithSpacesFindsMatches) {
  const char* text = "hello world";
  std::memcpy(process_->GetRawMemory().data() + 400, text, std::strlen(text));

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kString);
  model_->SetTargetScanValue(StringToBytes("hello wor"));

  Scan();

  VerifyAddressCount(1);
  VerifyAddresses({400});
  VerifyStride(9);
}

TEST_F(ScanResultModelTest, FirstScanStringWithSpacesAtUnalignedAddress) {
  const char* text = "hello world";
  std::memset(
      process_->GetRawMemory().data(), 0, process_->GetRawMemory().size());
  std::memcpy(process_->GetRawMemory().data() + 401, text, std::strlen(text));

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetScanValueType(ScanValueType::kString);
  model_->SetTargetScanValue(StringToBytes("hello wor"));

  Scan();

  VerifyAddressCount(1);
  VerifyAddresses({401});
}

// --- Logic Tests ---

TEST_F(ScanResultModelLogicTest,
       UnknownScanFindsUnalignedWhenFastScanDisabled) {
  model_->SetFastScan(false);

  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);

  EXPECT_GT(model_->entries().addresses.size(), 8000);
  VerifyStride(sizeof(uint32_t));

  uintptr_t base = process_->GetBaseAddress();
  EXPECT_EQ(model_->entries().addresses[0], base + 0);
  EXPECT_EQ(model_->entries().addresses[1], base + 1);
  EXPECT_EQ(model_->entries().addresses[2], base + 2);
  EXPECT_EQ(model_->entries().addresses[3], base + 3);
}

TEST_F(ScanResultModelLogicTest, UnknownScanSnapshotsAcrossChunks) {
  PerformFirstScan<uint32_t>(ScanComparison::kUnknown);

  VerifyAddressCount(2048);

  uintptr_t base = process_->GetBaseAddress();
  EXPECT_EQ(model_->entries().addresses[0], base + 0);
  EXPECT_EQ(model_->entries().addresses[1023], base + 4092);
  EXPECT_EQ(model_->entries().addresses[1024], base + 4096);
  EXPECT_EQ(model_->entries().addresses[2047], base + 8188);

  for (size_t i = 0; i < model_->entries().addresses.size(); ++i) {
    EXPECT_EQ(model_->entries().addresses[i], base + (i * 4))
        << "Gap found at index " << i;
  }
}

// --- Chunked Tests ---

TEST_F(ScanResultModelChunkedTest, FindsMatchCrossingChunkBoundary) {
  constexpr auto kChunkSize = static_cast<size_t>(32 * 1024 * 1024);
  const size_t near_boundary_offset = kChunkSize - 4;
  const uint32_t magic_value = 0xDEADBEEF;

  WriteValue<uint32_t>(near_boundary_offset, magic_value);
  WriteValue<uint32_t>(100, magic_value);
  WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, magic_value);

  VerifyAddressCount(3);

  uintptr_t base = process_->GetBaseAddress();
  bool found_near_boundary = false;
  for (auto addr : model_->entries().addresses) {
    if (addr == base + near_boundary_offset) {
      found_near_boundary = true;
    }
  }
  EXPECT_TRUE(found_near_boundary)
      << "Failed to find match near 32MB chunk boundary!";
}

TEST_F(ScanResultModelChunkedTest, ExactScanSkipsUnalignedAddresses) {
  const uint32_t magic_value = 0xCAFEBABE;

  WriteValue<uint32_t>(0, magic_value);
  WriteValue<uint32_t>(100, magic_value);
  WriteValue<uint32_t>(1000, magic_value);

  WriteValue<uint32_t>(201, magic_value);
  WriteValue<uint32_t>(307, magic_value);
  WriteValue<uint32_t>(503, magic_value);

  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, magic_value);

  VerifyAddressCount(3);
  VerifyAddresses({0, 100, 1000});

  uintptr_t base = process_->GetBaseAddress();
  for (auto addr : model_->entries().addresses) {
    EXPECT_EQ((addr - base) % 4, 0) << "Found unaligned address";
  }
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
  constexpr auto kChunkSize = static_cast<size_t>(32 * 1024 * 1024);
  const uint32_t magic_value = 0xBEEFCAFE;

  WriteValue<uint32_t>(0, magic_value);
  WriteValue<uint32_t>(kChunkSize, magic_value);
  WriteValue<uint32_t>(kChunkSize + 100, magic_value);

  WriteValue<uint32_t>(kChunkSize + 201, magic_value);
  WriteValue<uint32_t>(kChunkSize + 303, magic_value);

  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, magic_value);

  VerifyAddressCount(3);
  VerifyAddresses({0, kChunkSize, kChunkSize + 100});
}

TEST_F(ScanResultModelChunkedTest, FindsUnalignedWhenFastScanDisabled) {
  const uint32_t magic_value = 0xCAFEBABE;

  WriteValue<uint32_t>(1, magic_value);
  WriteValue<uint32_t>(13, magic_value);

  model_->SetFastScan(false);
  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, magic_value);

  VerifyAddressCount(2);
  VerifyAddresses({1, 13});
}

TEST_F(ScanResultModelChunkedTest, DestructorDoesNotHangWhenScanning) {
  StartScanWithoutWaiting(ScanComparison::kUnknown);
  model_.reset();
}

TEST_F(ScanResultModelTest, CommittedConfigMatchesScanTimeSettings) {
  WriteValue<uint32_t>(100, 42);

  model_->SetScanComparison(ScanComparison::kExactValue);
  model_->SetTargetScanValue(ToBytes<uint32_t>(42));
  model_->FirstScan();

  model_->SetTargetScanValue(ToBytes<uint32_t>(99));
  WaitForScanComplete();

  EXPECT_EQ(GetCommittedValue<uint32_t>(), 42)
      << "Committed config should use the value from scan start (42), not the "
         "value changed mid-scan (99)";
}

TEST_F(ScanResultModelTest, NextScanAfterChangeTypeShouldNotCrash) {
  WriteValue<uint32_t>(100, 42);
  PerformFirstScan<uint32_t>(ScanComparison::kExactValue, 42);
  VerifyAddressCount(1);

  // Change type to uint16
  model_->ChangeResultType(ScanValueType::kUInt16);
  model_->SetTargetScanValue(
      {});  // Avoid validation failure due to old 4-byte value

  // This next scan uses previous values. If the previous values buffer
  // wasn't resized correctly during ChangeResultType, this will crash.
  PerformNextScan<uint16_t>(ScanComparison::kChanged);

  // Implicit success if we reach here without crash
  SUCCEED();
}

}  // namespace
}  // namespace maia
