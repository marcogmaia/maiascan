#pragma once

#include <vector>

#include <tl/optional.hpp>

#include "maiascan/scanner/types.h"

namespace maia {

struct Match {
  using Offset = uint32_t;
  using Offsets = std::vector<Offset>;

  MemoryPage page;
  Offsets offsets;
};

using Matches = std::vector<Match>;

}  // namespace maia
