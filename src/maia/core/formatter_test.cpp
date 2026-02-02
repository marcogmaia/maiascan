// Copyright (c) Maia

#include <gtest/gtest.h>
#include <vector>
#include "maia/core/address_formatter.h"
#include "maia/core/value_formatter.h"

namespace maia {
namespace {

TEST(AddressFormatterTest, FormatAbsoluteAddress) {
  AddressFormatter formatter({});
  auto result = formatter.Format(0x12345678);
  EXPECT_EQ(result.text, "0x12345678");
  EXPECT_FALSE(result.is_relative);
}

TEST(AddressFormatterTest, FormatRelativeAddress) {
  std::vector<mmem::ModuleDescriptor> modules{
      mmem::ModuleDescriptor{.base = 0x1000,
                             .end = 0x2000,
                             .size = 0x1000,
                             .path = "C:\\test.exe",
                             .name = "test.exe"}
  };

  AddressFormatter formatter(modules);

  auto result = formatter.Format(0x1500);
  EXPECT_EQ(result.text, "test.exe+0x500");
  EXPECT_TRUE(result.is_relative);
}

TEST(AddressFormatterTest, FormatBoundaryAddress) {
  std::vector<mmem::ModuleDescriptor> modules{
      mmem::ModuleDescriptor{.base = 0x1000,
                             .end = 0x2000,
                             .size = 0x1000,
                             .path = "C:\\test.exe",
                             .name = "test.exe"}
  };

  AddressFormatter formatter(modules);

  // Exactly at start
  auto result_start = formatter.Format(0x1000);
  EXPECT_EQ(result_start.text, "test.exe+0x0");

  // Exactly at end (should be absolute as module range is usually [start, end))
  auto result_end = formatter.Format(0x2000);
  EXPECT_EQ(result_end.text, "0x2000");
}

TEST(ValueFormatterTest, FormatWithInsufficientData) {
  std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}};

  // Try to format 2 bytes as Int32 (4 bytes)
  auto result = ValueFormatter::Format(data, ScanValueType::kInt32, false);
  EXPECT_EQ(result, "Invalid");
}

TEST(ValueFormatterTest, FormatEmptyData) {
  auto result = ValueFormatter::Format({}, ScanValueType::kInt32, false);
  EXPECT_EQ(result, "N/A");
}

TEST(ValueFormatterTest, FormatInt32) {
  int32_t val = 123456;
  std::vector<std::byte> data(4);
  std::memcpy(data.data(), &val, 4);

  auto result = ValueFormatter::Format(data, ScanValueType::kInt32, false);
  EXPECT_EQ(result, "123456");
}

}  // namespace
}  // namespace maia
