// Copyright (c) Maia

#include "maia/core/string_utils.h"

#include <gtest/gtest.h>

namespace maia::core {

TEST(StringUtilsTest, Trim) {
  EXPECT_EQ(Trim("  hello  "), "hello");
  EXPECT_EQ(Trim("hello"), "hello");
  EXPECT_EQ(Trim("  hello"), "hello");
  EXPECT_EQ(Trim("hello  "), "hello");
  EXPECT_EQ(Trim("\t\r\nhello\t\r\n"), "hello");
  EXPECT_EQ(Trim(""), "");
  EXPECT_EQ(Trim("   "), "");
}

TEST(StringUtilsTest, Split) {
  std::vector<std::string_view> result;

  result = Split("hello,world,test", ',');
  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], "hello");
  EXPECT_EQ(result[1], "world");
  EXPECT_EQ(result[2], "test");

  result = Split("hello", ',');
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], "hello");

  result = Split("", ',');
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], "");

  result = Split("hello,,world", ',');
  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], "hello");
  EXPECT_EQ(result[1], "");
  EXPECT_EQ(result[2], "world");
}

TEST(StringUtilsTest, ParseNumberInteger) {
  EXPECT_EQ(ParseNumber<int>("123"), 123);
  EXPECT_EQ(ParseNumber<int>("-123"), -123);
  EXPECT_EQ(ParseNumber<int>("  123  "), 123);
  EXPECT_EQ(ParseNumber<int>("0"), 0);

  // Hex
  EXPECT_EQ(ParseNumber<int>("0xFF", 0), 255);
  EXPECT_EQ(ParseNumber<int>("0xff", 0), 255);
  EXPECT_EQ(ParseNumber<int>("FF", 16), 255);
  EXPECT_EQ(ParseNumber<unsigned int>("FFFFFFFF", 16), 0xFFFFFFFF);

  // Invalid
  EXPECT_EQ(ParseNumber<int>("abc"), std::nullopt);
  EXPECT_EQ(ParseNumber<int>("123a"),
            std::nullopt);  // Partial match should fail
  EXPECT_EQ(ParseNumber<int>(""), std::nullopt);
}

TEST(StringUtilsTest, ParseNumberFloat) {
  auto res = ParseNumber<float>("3.14");
  ASSERT_TRUE(res.has_value());
  EXPECT_FLOAT_EQ(*res, 3.14f);

  res = ParseNumber<float>("-0.5");
  ASSERT_TRUE(res.has_value());
  EXPECT_FLOAT_EQ(*res, -0.5f);

  res = ParseNumber<float>("1.0e-3");
  ASSERT_TRUE(res.has_value());
  EXPECT_FLOAT_EQ(*res, 0.001f);
}

TEST(StringUtilsTest, ToStringInteger) {
  EXPECT_EQ(ToString(123), "123");
  EXPECT_EQ(ToString(-456), "-456");
  EXPECT_EQ(ToString(0), "0");
  EXPECT_EQ(ToString(0xFF), "255");
}

TEST(StringUtilsTest, ToStringUnsigned) {
  EXPECT_EQ(ToString(123u), "123");
  EXPECT_EQ(ToString(0u), "0");
}

TEST(StringUtilsTest, ToHexString) {
  EXPECT_EQ(ToHexString(255), "ff");
  EXPECT_EQ(ToHexString(0), "0");
  EXPECT_EQ(ToHexString(0xDEADBEEF), "deadbeef");
  EXPECT_EQ(ToHexString(0xAB), "ab");
}

TEST(StringUtilsTest, ToHexStringUppercase) {
  EXPECT_EQ(ToHexString(255, true), "FF");
  EXPECT_EQ(ToHexString(0xDEADBEEF, true), "DEADBEEF");
}

TEST(StringUtilsTest, FormatAddressHex) {
  EXPECT_EQ(FormatAddressHex(0xFFFFFF), "0x00FFFFFF");
  EXPECT_EQ(FormatAddressHex(0xFFDEADBEEF), "0x000000FFDEADBEEF");
}

}  // namespace maia::core
