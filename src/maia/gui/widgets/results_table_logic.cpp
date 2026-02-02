// Copyright (c) Maia

#include "maia/gui/widgets/results_table_logic.h"

#include <algorithm>

namespace maia {

bool ResultsTableLogic::ShouldHighlightValue(std::span<const std::byte> curr,
                                             std::span<const std::byte> prev) {
  if (prev.empty() || curr.empty()) {
    return false;
  }

  if (curr.size() != prev.size()) {
    return false;
  }

  return !std::ranges::equal(curr, prev);
}

}  // namespace maia
