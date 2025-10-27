#pragma once

#include <vector>

#include "maia/scanner/types.h"

namespace maia {

struct Match {
  using Offset = uint32_t;
  using Offsets = std::vector<Offset>;

  Page page;
  Offsets offsets;
};

using Matches = std::vector<Match>;

}  // namespace maia
