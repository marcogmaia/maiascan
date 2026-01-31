// Copyright (c) Maia

#include <span>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "maia/application/cheat_table_model.h"
#include "maia/core/i_process.h"
#include "maia/tests/fake_process.h"
#include "maia/tests/task_runner.h"

namespace maia {

class MockProcess : public IProcess {
 public:
  // IProcess Interface
  MOCK_METHOD(bool, IsProcessValid, (), (const, override));
  MOCK_METHOD(std::string, GetProcessName, (), (const, override));
  MOCK_METHOD(uint32_t, GetProcessId, (), (const, override));
  MOCK_METHOD(uintptr_t, GetBaseAddress, (), (const, override));

  MOCK_METHOD(bool,
              ReadMemory,
              (std::span<const MemoryAddress> addresses,
               size_t bytes_per_address,
               std::span<std::byte> out_buffer,
               std::vector<uint8_t>* success_mask),
              (override));

  MOCK_METHOD(bool,
              WriteMemory,

              (uintptr_t address, std::span<const std::byte> buffer),
              (override));

  MOCK_METHOD(std::vector<MemoryRegion>,
              GetMemoryRegions,
              (),
              (const, override));

  MOCK_METHOD(std::vector<mmem::ModuleDescriptor>,
              GetModules,
              (),
              (const, override));

  MOCK_METHOD(bool, Suspend, (), (override));
  MOCK_METHOD(bool, Resume, (), (override));
  MOCK_METHOD(size_t, GetPointerSize, (), (const, override));
};

// Test fixture for verifying concurrency fixes
class CheatTableModelTest : public ::testing::Test {
 protected:
  testing::NiceMock<MockProcess> mock_process_;
  CheatTableModel model_{std::make_unique<test::NoOpTaskRunner>()};

  void CallWriteMemory(size_t index, const std::vector<std::byte>& data) {
    // We use "1" here because our entry is kInt32.
    // kInt32 will parse "1" into {0x01, 0x00, 0x00, 0x00}.
    // The test mock expects data of size 4 if we use ReadMemory size... wait.
    // The test uses CallWriteMemory(0, data).
    // The logic inside CallWriteMemory originally called private WriteMemory
    // directly. Now we call SetValue("1").
    model_.SetValue(index, "1");
  }
};

TEST_F(CheatTableModelTest, HandlesUnreadableMemoryForVariableSizeEntries) {
  // Setup Mock
  EXPECT_CALL(mock_process_, IsProcessValid())
      .WillRepeatedly(testing::Return(true));

  // First read succeeds (during AddEntry), subsequent reads fail
  EXPECT_CALL(mock_process_,
              ReadMemory(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(true))          // AddEntry
      .WillRepeatedly(testing::Return(false));  // UpdateValues

  // Add a string entry (size 10)
  model_.SetActiveProcess(&mock_process_);
  model_.AddEntry(0x1000, ScanValueType::kString, "String Entry", 10);

  auto entries = model_.entries();
  ASSERT_EQ(entries->size(), 1);
  EXPECT_EQ(entries->at(0).data->GetValueSize(), 10);

  // Trigger update - should not crash or change value if read fails
  // Since UpdateValues uses stack buffer and memcmp/memcpy only on success,
  // it should remain unchanged.
  std::vector<std::byte> original_value = entries->at(0).data->GetValue();
  model_.UpdateValues();

  EXPECT_EQ(entries->at(0).data->GetValue(), original_value);
}

TEST_F(CheatTableModelTest, EnforcesStringSizeSafetyOnSetValue) {
  EXPECT_CALL(mock_process_, IsProcessValid())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_process_, WriteMemory(testing::_, testing::_))
      .WillRepeatedly(testing::Return(true));

  // Add a string entry with size 5
  model_.SetActiveProcess(&mock_process_);
  model_.AddEntry(0x1000, ScanValueType::kString, "String Entry", 5);

  auto entries = model_.entries();
  ASSERT_EQ(entries->size(), 1);

  // Try to set a longer string
  model_.SetValue(0, "12345678");
  {
    auto value = entries->at(0).data->GetValue();
    EXPECT_EQ(value.size(), 5);
    std::string val(reinterpret_cast<const char*>(value.data()), 5);
    EXPECT_EQ(val, "12345");
  }

  // 2. Try to set a shorter string
  model_.SetValue(0, "ABC");
  {
    auto value = entries->at(0).data->GetValue();
    EXPECT_EQ(value.size(), 5);
    EXPECT_EQ(static_cast<char>(value[0]), 'A');
    EXPECT_EQ(static_cast<char>(value[1]), 'B');
    EXPECT_EQ(static_cast<char>(value[2]), 'C');
    EXPECT_EQ(static_cast<char>(value[3]), '\0');
    EXPECT_EQ(static_cast<char>(value[4]), '\0');
  }
}

TEST_F(CheatTableModelTest, FrozenValueIsReappliedWhenProcessChangesIt) {
  // Setup
  EXPECT_CALL(mock_process_, IsProcessValid())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_process_,
              ReadMemory(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::Return(true));

  model_.SetActiveProcess(&mock_process_);
  model_.AddEntry(0x1000, ScanValueType::kInt32, "Health");
  const auto& entry = *model_.entries();

  // Set Value to 100.
  model_.SetValue(0, "100");

  // Freeze.
  model_.ToggleFreeze(0);

  // Verify that UpdateValues calls WriteMemory with 100 effectively undoing any
  // external change.
  EXPECT_CALL(mock_process_, WriteMemory(0x1000, testing::_))
      .WillOnce([&](uintptr_t, std::span<const std::byte> data) {
        EXPECT_EQ(data.size(), 4);
        // 100 == 0x64
        EXPECT_EQ(data[0], std::byte{0x64});
        return true;
      });

  model_.UpdateValues();
}

TEST_F(CheatTableModelTest, FrozenValueIsHeldAgainstExternalChanges) {
  test::FakeProcess fake_process;
  model_.SetActiveProcess(&fake_process);

  const MemoryAddress addr = fake_process.GetBaseAddress();
  model_.AddEntry(addr, ScanValueType::kInt32, "Int Entry");

  // Set to 100 and freeze
  model_.SetValue(0, "100");
  model_.ToggleFreeze(0);

  // External change to 200
  // Note: We use offset 0 because addr == base_address
  fake_process.WriteValue<int32_t>(0, 200);

  // Manually trigger update (to avoid sleeping in tests)
  model_.UpdateValues();

  // Verify it's back to 100 in the "process" memory
  int32_t current_val = 0;
  std::span<std::byte> out_buf(reinterpret_cast<std::byte*>(&current_val), 4);
  fake_process.ReadMemory({&addr, 1}, 4, out_buf, nullptr);
  EXPECT_EQ(current_val, 100);
}

TEST_F(CheatTableModelTest, SetValueWhileFrozenUpdatesFrozenValue) {
  test::FakeProcess fake_process;
  model_.SetActiveProcess(&fake_process);
  const MemoryAddress addr = fake_process.GetBaseAddress();
  model_.AddEntry(addr, ScanValueType::kInt32, "Int Entry");

  model_.SetValue(0, "100");
  model_.ToggleFreeze(0);

  // Set to 200 while frozen
  model_.SetValue(0, "200");

  // Verify frozen value is now 200 in the process
  model_.UpdateValues();

  int32_t current_val = 0;
  std::span<std::byte> out_buf(reinterpret_cast<std::byte*>(&current_val), 4);
  fake_process.ReadMemory({&addr, 1}, 4, out_buf, nullptr);
  EXPECT_EQ(current_val, 200);
}

}  // namespace maia
