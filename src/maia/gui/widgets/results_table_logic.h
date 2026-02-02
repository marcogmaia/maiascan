// Copyright (c) Maia

#pragma once

#include <cstddef>
#include <span>

namespace maia {

/// \brief Logic for the results table view that can be tested without ImGui.
class ResultsTableLogic {
 public:
  /// \brief Returns true if the value has changed between current and previous.
  static bool ShouldHighlightValue(std::span<const std::byte> curr,
                                   std::span<const std::byte> prev);
};

}  // namespace maia
