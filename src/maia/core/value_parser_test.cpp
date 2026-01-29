// Copyright (c) Maia

#include "maia/core/value_parser.h"

#include <gtest/gtest.h>

namespace maia {

TEST(ValueParserTest, ParseInt32) {
  auto bytes = ParseStringByType("123", ScanValueType::kInt32);
  ASSERT_EQ(bytes.size(), 4);
  int32_t val;
  std::memcpy(&val, bytes.data(), 4);
  EXPECT_EQ(val, 123);
}

TEST(ValueParserTest, ParseString) {
  std::string input = "Hello World";
  auto bytes = ParseStringByType(input, ScanValueType::kString);

  ASSERT_FALSE(bytes.empty());
  EXPECT_EQ(bytes.size(), input.size());

  std::string output(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  EXPECT_EQ(output, input);
}

}  // namespace maia
