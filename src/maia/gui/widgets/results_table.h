// Copyright (c) Maia

#pragma once

#include <imgui.h>

#include "maia/core/address_formatter.h"
#include "maia/core/scan_types.h"

namespace maia {

/// \brief Mutable state and output for the results table.
struct ResultsTableState {
  int& selected_idx;
  bool& double_clicked;
  ScanValueType* out_new_type = nullptr;
  bool* out_is_hex = nullptr;
  uintptr_t* out_browse_address = nullptr;
};

class ResultsTable {
 public:
  void Render(const ScanStorage& data,
              const AddressFormatter& formatter,
              ScanValueType value_type,
              bool is_hex,
              ResultsTableState& state);
};

}  // namespace maia
