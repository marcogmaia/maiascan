// Copyright (c) Maia

#pragma once

#include <imgui.h>

#include "maia/core/scan_types.h"

namespace maia {

class ResultsTable {
 public:
  //  TODO: Check if it makes sense to treat this "signal-like" index and double
  //  clicking signaling from parameters, lets study a cleaner way.
  void Render(const ScanStorage& data,
              ScanValueType value_type,
              bool is_hex,
              int& selected_idx,
              bool& double_clicked);
};

}  // namespace maia
