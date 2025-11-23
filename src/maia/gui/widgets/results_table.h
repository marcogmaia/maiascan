// Copyright (c) Maia

#pragma once

#include <imgui.h>

#include "maia/core/scan_types.h"

namespace maia {

class ResultsTable {
 public:
  void Render(const ScanStorage& data,
              ScanValueType value_type,
              bool is_hex,
              int& selected_idx);
};

}  // namespace maia
