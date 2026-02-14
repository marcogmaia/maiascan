// Copyright (c) Maia

#include <gtest/gtest.h>

#include "maia/application/cheat_table_model.h"
#include "maia/tests/task_runner.h"

namespace maia {

TEST(CheatTableModelSerializationTest, RoundtripInMemoryWorks) {
  CheatTableModel model{std::make_unique<test::NoOpTaskRunner>()};
  // Add some entries
  model.AddEntry(0x1234, ScanValueType::kInt32, "Static Entry");
  model.SetShowAsHex(0, true);

  std::vector<int64_t> offsets = {0x10, 0x20};
  model.AddPointerChainEntry(0x5678,
                             offsets,
                             "test.exe",
                             0x100,
                             ScanValueType::kFloat,
                             "Dynamic Entry");

  // Save to stringstream
  std::stringstream ss;
  ASSERT_TRUE(model.Save(ss));

  // Create a new model and load from stringstream
  CheatTableModel new_model{std::make_unique<test::NoOpTaskRunner>()};
  ASSERT_TRUE(new_model.Load(ss));

  auto entries = new_model.entries();
  ASSERT_EQ(entries->size(), 2);

  // Verify first entry
  EXPECT_EQ(entries->at(0).address, 0x1234);
  EXPECT_EQ(entries->at(0).type, ScanValueType::kInt32);
  EXPECT_EQ(entries->at(0).description, "Static Entry");
  EXPECT_TRUE(entries->at(0).show_as_hex);

  // Verify second entry
  EXPECT_EQ(entries->at(1).pointer_base, 0x5678);
  EXPECT_EQ(entries->at(1).pointer_offsets, offsets);
  EXPECT_EQ(entries->at(1).pointer_module, "test.exe");
  EXPECT_EQ(entries->at(1).pointer_module_offset, 0x100);
  EXPECT_EQ(entries->at(1).type, ScanValueType::kFloat);
  EXPECT_EQ(entries->at(1).description, "Dynamic Entry");
  EXPECT_FALSE(entries->at(1).show_as_hex);
}

}  // namespace maia
