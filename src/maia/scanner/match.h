// Copyright (c) Maia

#pragma once

#include <vector>

#include "maia/core/memory_common.h"

namespace maia {

struct Match {
  using Offset = uint32_t;
  using Offsets = std::vector<Offset>;

  Page page;
  Offsets offsets;
};

using Matches = std::vector<Match>;

}  // namespace maia
