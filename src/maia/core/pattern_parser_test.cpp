// Copyright (c) Maia

#include "maia/core/pattern_parser.h"
#include <gtest/gtest.h>

namespace maia {

TEST(PatternParserTest, ParseAobSimple) {
  auto p = ParseAob("AA BB CC");
  ASSERT_EQ(p.value.size(), 3);
  ASSERT_EQ(p.mask.size(), 3);
  EXPECT_EQ(p.value[0], std::byte{0xAA});
  EXPECT_EQ(p.value[1], std::byte{0xBB});
  EXPECT_EQ(p.value[2], std::byte{0xCC});
  EXPECT_EQ(p.mask[0], std::byte{0xFF});
  EXPECT_EQ(p.mask[1], std::byte{0xFF});
  EXPECT_EQ(p.mask[2], std::byte{0xFF});
}

TEST(PatternParserTest, ParseAobWithWildcards) {
  auto p = ParseAob("AA ?? CC ? DD");
  ASSERT_EQ(p.value.size(), 5);
  EXPECT_EQ(p.value[0], std::byte{0xAA});
  EXPECT_EQ(p.mask[0], std::byte{0xFF});

  EXPECT_EQ(p.mask[1], std::byte{0x00});

  EXPECT_EQ(p.value[2], std::byte{0xCC});
  EXPECT_EQ(p.mask[2], std::byte{0xFF});

  EXPECT_EQ(p.mask[3], std::byte{0x00});

  EXPECT_EQ(p.value[4], std::byte{0xDD});
  EXPECT_EQ(p.mask[4], std::byte{0xFF});
}

TEST(PatternParserTest, ParseText) {
  auto p = ParseText("ABC", false);
  ASSERT_EQ(p.value.size(), 3);
  EXPECT_EQ(p.value[0], std::byte{'A'});
  EXPECT_EQ(p.mask[0], std::byte{0xFF});
}

TEST(PatternParserTest, ParseTextWithSpaces) {
  auto p = ParseText("A B C", false);
  ASSERT_EQ(p.value.size(), 5);
  EXPECT_EQ(p.value[0], std::byte{'A'});
  EXPECT_EQ(p.value[1], std::byte{' '});
  EXPECT_EQ(p.value[2], std::byte{'B'});
  EXPECT_EQ(p.value[3], std::byte{' '});
  EXPECT_EQ(p.value[4], std::byte{'C'});
  for (auto m : p.mask) {
    EXPECT_EQ(m, std::byte{0xFF});
  }
}

TEST(PatternParserTest, ParseAobWithQuotedString) {
  auto p = ParseAob("AA \"hello world\" BB");
  // AA (1) + "hello world" (11) + BB (1) = 13 bytes
  ASSERT_EQ(p.value.size(), 13);
  EXPECT_EQ(p.value[0], std::byte{0xAA});
  EXPECT_EQ(p.value[1], std::byte{'h'});
  EXPECT_EQ(p.value[6], std::byte{' '});
  EXPECT_EQ(p.value[11], std::byte{'d'});
  EXPECT_EQ(p.value[12], std::byte{0xBB});
}

TEST(PatternParserTest, ParseTextUnicode) {
  auto p = ParseText("A", true);
  ASSERT_EQ(p.value.size(), 2);
  EXPECT_EQ(p.value[0], std::byte{'A'});
  EXPECT_EQ(p.value[1], std::byte{0});
  EXPECT_EQ(p.mask[0], std::byte{0xFF});
  EXPECT_EQ(p.mask[1], std::byte{0xFF});
}

}  // namespace maia
