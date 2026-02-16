// Copyright (c) Maia

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "maia/application/cheat_table_model.h"
#include "maia/application/pointer_scanner_model.h"
#include "maia/application/pointer_scanner_viewmodel.h"
#include "maia/application/process_model.h"
#include "maia/application/scan_result_model.h"
#include "maia/core/pointer_scanner.h"
#include "maia/gui/models/ui_state.h"
#include "maia/tests/fake_process.h"
#include "maia/tests/task_runner.h"

namespace maia {

namespace {

// Regression test: Verify that OnResultDoubleClicked adds a pointer chain
// entry that preserves the full path information for dynamic resolution.
// Before the fix, it would add a static entry with the resolved address only,
// losing all pointer chain information.
class PointerScannerViewModelRegressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    process_model_ = std::make_unique<ProcessModel>();
    cheat_table_model_ = std::make_unique<CheatTableModel>(
        std::make_unique<test::NoOpTaskRunner>());
    scan_result_model_ = std::make_unique<ScanResultModel>();
    state_ = std::make_unique<gui::PointerScannerState>();

    // Use the real scanner model - we'll set up paths directly
    scanner_model_ = std::make_unique<PointerScannerModel>();

    viewmodel_ = std::make_unique<PointerScannerViewModel>(*scanner_model_,
                                                           *process_model_,
                                                           *cheat_table_model_,
                                                           *scan_result_model_,
                                                           *state_);
  }

  std::unique_ptr<PointerScannerModel> scanner_model_;
  std::unique_ptr<ProcessModel> process_model_;
  std::unique_ptr<CheatTableModel> cheat_table_model_;
  std::unique_ptr<ScanResultModel> scan_result_model_;
  std::unique_ptr<gui::PointerScannerState> state_;
  std::unique_ptr<PointerScannerViewModel> viewmodel_;
};

TEST_F(PointerScannerViewModelRegressionTest,
       OnResultDoubleClickedAddsPointerChainEntryNotStaticEntry) {
  // Arrange: Create a pointer path with full information
  // Note: FakeProcess has base_address_ = 0x100000, so we need to use
  // addresses >= 0x100000
  core::PointerPath test_path;
  // When module_name is set, base_address is ignored and the address is
  // calculated as: module_base + module_offset
  test_path.base_address = 0;  // Ignored when module_name is set
  test_path.module_name = "game.exe";
  test_path.module_offset = 0x100;  // 0x100000 + 0x100 = 0x100100
  test_path.offsets = {0x10, 0x20, 0x30};

  // Create a fake process with enough memory for our test
  // Memory layout (all offsets relative to base_address_ = 0x100000):
  // 0x100100 -> points to 0x100200
  // 0x100200 + 0x10 = 0x100210 -> points to 0x100400
  // 0x100400 + 0x20 = 0x100420 -> points to 0x100600
  // 0x100600 + 0x30 = 0x100630 -> final value
  test::FakeProcess fake_process(0x1000, 8);  // 4KB memory, 8-byte pointers

  // Add the module to the fake process so resolution works
  // Module base is 0x100000 (same as process base), offset 0x100 gives 0x100100
  fake_process.AddModule("game.exe", 0x100000, 0x800);

  fake_process.WriteValue<uint64_t>(0x100, 0x100200);
  fake_process.WriteValue<uint64_t>(0x210, 0x100400);
  fake_process.WriteValue<uint64_t>(0x420, 0x100600);
  fake_process.WriteValue<int32_t>(0x630, 0x12345678);

  // Set up the scanner model
  scanner_model_->SetActiveProcess(&fake_process);
  scanner_model_->SetTargetType(ScanValueType::kInt32);
  scanner_model_->SetPaths({test_path});

  // Act: Simulate double-click on the first result
  viewmodel_->OnResultDoubleClicked(0);

  // Assert: Verify entry was added to cheat table
  auto entries = cheat_table_model_->entries();
  ASSERT_EQ(entries->size(), 1) << "Expected exactly one entry in cheat table";

  const auto& entry = entries->at(0);

  // CRITICAL: The entry should be a dynamic/pointer chain entry, not static
  EXPECT_TRUE(entry.IsDynamicAddress())
      << "Entry should be marked as dynamic address";

  // Verify all pointer chain information is preserved
  EXPECT_EQ(entry.pointer_base, test_path.base_address)
      << "Base address should be preserved";
  EXPECT_EQ(entry.pointer_module, test_path.module_name)
      << "Module name should be preserved";
  EXPECT_EQ(entry.pointer_module_offset, test_path.module_offset)
      << "Module offset should be preserved";
  EXPECT_EQ(entry.pointer_offsets, test_path.offsets)
      << "Offsets should be preserved";

  // Verify entry type and description
  EXPECT_EQ(entry.type, ScanValueType::kInt32);
  EXPECT_EQ(entry.description, "Pointer Path Result");

  // Verify static address is 0 (it's resolved dynamically)
  EXPECT_EQ(entry.address, 0)
      << "Static address should be 0 for dynamic entries";
}

TEST_F(PointerScannerViewModelRegressionTest,
       OnResultDoubleClickedPreservesNegativeOffsets) {
  // Arrange: Create a path with negative offsets (common in real games)
  core::PointerPath test_path;
  test_path.base_address = 0;
  test_path.module_name = "game.exe";
  test_path.module_offset = 0x400;
  test_path.offsets = {0x10, -0x8, 0x20};

  // Memory layout with negative offset:
  // 0x100400 -> points to 0x100500
  // 0x100500 + 0x10 = 0x100510 -> points to 0x100700
  // 0x100700 - 0x8 = 0x1006F8 -> points to 0x100800
  // 0x100800 + 0x20 = 0x100820 -> final value
  test::FakeProcess fake_process(0x1000, 8);
  fake_process.AddModule("game.exe", 0x100000, 0x900);

  fake_process.WriteValue<uint64_t>(0x400, 0x100500);
  fake_process.WriteValue<uint64_t>(0x510, 0x100700);
  fake_process.WriteValue<uint64_t>(0x6F8, 0x100800);
  fake_process.WriteValue<int64_t>(0x820, 0xABCDEF);

  scanner_model_->SetActiveProcess(&fake_process);
  scanner_model_->SetTargetType(ScanValueType::kInt64);
  scanner_model_->SetPaths({test_path});

  // Act
  viewmodel_->OnResultDoubleClicked(0);

  // Assert
  auto entries = cheat_table_model_->entries();
  ASSERT_EQ(entries->size(), 1);

  const auto& entry = entries->at(0);
  EXPECT_TRUE(entry.IsDynamicAddress());
  EXPECT_EQ(entry.pointer_offsets.size(), 3);
  EXPECT_EQ(entry.pointer_offsets[0], 0x10);
  EXPECT_EQ(entry.pointer_offsets[1], -0x8);
  EXPECT_EQ(entry.pointer_offsets[2], 0x20);
}

TEST_F(PointerScannerViewModelRegressionTest,
       OnResultDoubleClickedDoesNothingWhenResolveFails) {
  // Arrange: Setup path that fails to resolve (no process set)
  core::PointerPath test_path;
  test_path.base_address = 0x100100;
  test_path.offsets = {0x10};

  scanner_model_->SetPaths({test_path});
  // No process set, so resolution will fail

  // Act
  viewmodel_->OnResultDoubleClicked(0);

  // Assert: No entry should be added when resolution fails
  auto entries = cheat_table_model_->entries();
  EXPECT_EQ(entries->size(), 0);
}

TEST_F(PointerScannerViewModelRegressionTest,
       OnResultDoubleClickedDoesNothingForInvalidIndex) {
  // Arrange: Empty paths list
  scanner_model_->SetPaths({});

  // Act: Try to access index 0 when no paths exist
  viewmodel_->OnResultDoubleClicked(0);

  // Assert: No entry should be added
  auto entries = cheat_table_model_->entries();
  EXPECT_EQ(entries->size(), 0);
}

TEST_F(PointerScannerViewModelRegressionTest,
       PointerChainEntrySurvivesSerialization) {
  // Arrange: Create a path and add it
  core::PointerPath test_path;
  test_path.base_address = 0;
  test_path.module_name = "module.dll";
  test_path.module_offset = 0x200;
  test_path.offsets = {0x10, 0x20, -0x8};

  // Memory layout:
  // 0x100200 -> points to 0x100300
  // 0x100300 + 0x10 = 0x100310 -> points to 0x100500
  // 0x100500 + 0x20 = 0x100520 -> points to 0x100700
  // 0x100700 - 0x8 = 0x1006F8 -> final value
  test::FakeProcess fake_process(0x1000, 8);
  fake_process.AddModule("module.dll", 0x100000, 0x900);

  fake_process.WriteValue<uint64_t>(0x200, 0x100300);
  fake_process.WriteValue<uint64_t>(0x310, 0x100500);
  fake_process.WriteValue<uint64_t>(0x520, 0x100700);
  fake_process.WriteValue<float>(0x6F8, 3.14159f);

  scanner_model_->SetActiveProcess(&fake_process);
  scanner_model_->SetTargetType(ScanValueType::kFloat);
  scanner_model_->SetPaths({test_path});

  viewmodel_->OnResultDoubleClicked(0);

  // Verify entry exists
  auto entries_before = cheat_table_model_->entries();
  ASSERT_EQ(entries_before->size(), 1);

  // Act: Save and reload the cheat table
  std::stringstream ss;
  ASSERT_TRUE(cheat_table_model_->Save(ss));

  CheatTableModel new_model{std::make_unique<test::NoOpTaskRunner>()};
  ASSERT_TRUE(new_model.Load(ss));

  // Assert: Verify all pointer chain info survives serialization
  auto entries_after = new_model.entries();
  ASSERT_EQ(entries_after->size(), 1);

  const auto& entry = entries_after->at(0);
  EXPECT_TRUE(entry.IsDynamicAddress());
  EXPECT_EQ(entry.pointer_base, test_path.base_address);
  EXPECT_EQ(entry.pointer_module, test_path.module_name);
  EXPECT_EQ(entry.pointer_module_offset, test_path.module_offset);
  EXPECT_EQ(entry.pointer_offsets, test_path.offsets);
  EXPECT_EQ(entry.type, ScanValueType::kFloat);
}

}  // namespace

}  // namespace maia
