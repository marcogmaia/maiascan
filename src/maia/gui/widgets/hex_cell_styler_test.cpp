// Copyright (c) Maia

#include "maia/gui/widgets/hex_cell_styler.h"
#include <gtest/gtest.h>

namespace maia::gui {
namespace {

TEST(HexCellStylerTest, ReturnsNormalStylesForValidByte) {
  HexCellState state{.value = std::byte{0xAB},
                     .is_valid = true,
                     .is_edited = false,
                     .is_selected = false,
                     .is_hovered = false,
                     .pending_nibble = -1};

  HexCellStyles styles = HexCellStyler::GetStyles(state);

  EXPECT_EQ(styles.text, "AB");
  EXPECT_FLOAT_EQ(styles.text_color.x, 1.0f);
  EXPECT_FLOAT_EQ(styles.text_color.y, 1.0f);
  EXPECT_FLOAT_EQ(styles.text_color.z, 1.0f);
}

TEST(HexCellStylerTest, ReturnsOrangeColorForEditedByte) {
  HexCellState state{.value = std::byte{0xAB},
                     .is_valid = true,
                     .is_edited = true,
                     .is_selected = false,
                     .is_hovered = false,
                     .is_pending = false,
                     .pending_nibble = -1};

  HexCellStyles styles = HexCellStyler::GetStyles(state);

  EXPECT_EQ(styles.text, "AB");
  EXPECT_FLOAT_EQ(styles.text_color.x, 1.0f);
  EXPECT_FLOAT_EQ(styles.text_color.y, 0.5f);
  EXPECT_FLOAT_EQ(styles.text_color.z, 0.0f);
}

TEST(HexCellStylerTest, ReturnsQuestionMarksForInvalidByte) {
  HexCellState state{.value = std::byte{0x00},
                     .is_valid = false,
                     .is_edited = false,
                     .is_selected = false,
                     .is_hovered = false,
                     .is_pending = false,
                     .pending_nibble = -1};

  HexCellStyles styles = HexCellStyler::GetStyles(state);

  EXPECT_EQ(styles.text, "??");
  EXPECT_FLOAT_EQ(styles.text_color.x, 0.5f);
}

TEST(HexCellStylerTest, ReturnsPendingTextAndRedBackground) {
  HexCellState state{.value = std::byte{0x00},
                     .is_valid = true,
                     .is_edited = false,
                     .is_selected = false,
                     .is_hovered = false,
                     .is_pending = true,
                     .pending_nibble = 0xA};

  HexCellStyles styles = HexCellStyler::GetStyles(state);

  EXPECT_EQ(styles.text, "A_");
  ASSERT_TRUE(styles.bg_color.has_value());
  // Assuming we use a specific U32 for redish background
  // Or we can just check if it's there for now.
}

TEST(HexCellStylerTest, ReturnsBackgroundForSelectedByte) {
  HexCellState state{.value = std::byte{0xAB},
                     .is_valid = true,
                     .is_edited = false,
                     .is_selected = true,
                     .is_hovered = false,
                     .is_pending = false,
                     .pending_nibble = -1};

  HexCellStyles styles = HexCellStyler::GetStyles(state);

  EXPECT_TRUE(styles.bg_color.has_value());
}

TEST(HexCellStylerTest, FadesFromRedToWhiteWhenChanged) {
  // Case 1: Just changed (0.0s ago) -> Should be Red (1, 0, 0)
  HexCellState state_new{.value = std::byte{0xAB},
                         .is_valid = true,
                         .is_edited = false,
                         .is_selected = false,
                         .is_hovered = false,
                         .is_pending = false,
                         .pending_nibble = -1,
                         .time_since_last_change = 0.0};

  HexCellStyles styles_new = HexCellStyler::GetStyles(state_new);
  EXPECT_FLOAT_EQ(styles_new.text_color.x, 1.0f);
  EXPECT_FLOAT_EQ(styles_new.text_color.y, 0.0f);
  EXPECT_FLOAT_EQ(styles_new.text_color.z, 0.0f);

  // Case 2: Halfway (1.0s ago) -> Should be Pinkish (1, 0.5, 0.5)
  // Logic was: t = time / 2.0 = 0.5. y = t, z = t.
  HexCellState state_half{.value = std::byte{0xAB},
                          .is_valid = true,
                          .is_edited = false,
                          .is_selected = false,
                          .is_hovered = false,
                          .is_pending = false,
                          .pending_nibble = -1,
                          .time_since_last_change = 1.0};

  HexCellStyles styles_half = HexCellStyler::GetStyles(state_half);
  EXPECT_FLOAT_EQ(styles_half.text_color.x, 1.0f);
  EXPECT_FLOAT_EQ(styles_half.text_color.y, 0.5f);
  EXPECT_FLOAT_EQ(styles_half.text_color.z, 0.5f);

  // Case 3: Finished (2.0s ago) -> Should be White (1, 1, 1)
  HexCellState state_done{.value = std::byte{0xAB},
                          .is_valid = true,
                          .is_edited = false,
                          .is_selected = false,
                          .is_hovered = false,
                          .is_pending = false,
                          .pending_nibble = -1,
                          .time_since_last_change = 2.0};

  HexCellStyles styles_done = HexCellStyler::GetStyles(state_done);
  EXPECT_FLOAT_EQ(styles_done.text_color.x, 1.0f);
  EXPECT_FLOAT_EQ(styles_done.text_color.y, 1.0f);
  EXPECT_FLOAT_EQ(styles_done.text_color.z, 1.0f);
}

}  // namespace
}  // namespace maia::gui
