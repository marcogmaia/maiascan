// Copyright (c) Maia

#include "maia/gui/widgets/results_table_logic.h"
#include <gtest/gtest.h>
#include <vector>

namespace maia {
namespace {

TEST(ResultsTableLogicTest, ShouldHighlightValueReturnsTrueWhenValuesDiffer) {
  std::vector<std::byte> curr = {std::byte{1}, std::byte{2}};
  std::vector<std::byte> prev = {std::byte{1}, std::byte{3}};

  EXPECT_TRUE(ResultsTableLogic::ShouldHighlightValue(curr, prev));
}

TEST(ResultsTableLogicTest,
     ShouldHighlightValueReturnsFalseWhenValuesAreEqual) {
  std::vector<std::byte> curr = {std::byte{1}, std::byte{2}};
  std::vector<std::byte> prev = {std::byte{1}, std::byte{2}};

  EXPECT_FALSE(ResultsTableLogic::ShouldHighlightValue(curr, prev));
}

TEST(ResultsTableLogicTest,
     ShouldHighlightValueReturnsFalseWhenPreviousIsEmpty) {
  std::vector<std::byte> curr = {std::byte{1}, std::byte{2}};
  std::vector<std::byte> prev = {};

  EXPECT_FALSE(ResultsTableLogic::ShouldHighlightValue(curr, prev));
}

TEST(ResultsTableLogicTest, ShouldHighlightValueReturnsFalseWhenSizesDiffer) {
  std::vector<std::byte> curr = {std::byte{1}, std::byte{2}};
  std::vector<std::byte> prev = {std::byte{1}};

  EXPECT_FALSE(ResultsTableLogic::ShouldHighlightValue(curr, prev));
}

}  // namespace
}  // namespace maia
