// Copyright (c) Maia

#pragma once

#include <string>
#include <vector>

#include "maia/core/memory_common.h"
#include "maia/core/scan_types.h"

namespace maia::gui {

struct ProcessSelectorState {
  bool is_visible = false;
  std::vector<ProcessInfo> processes;
  std::string attached_process_name = "N/A";
  Pid attached_pid = 0;
};

struct ScannerState {
  float progress = 0.0f;
  bool is_scanning = false;
};

struct CheatTableState {
  // Add fields if needed
};

struct PointerScannerState {
  bool is_visible = false;
  float map_progress = 0.0f;
  float scan_progress = 0.0f;
  bool is_generating_map = false;
  bool is_scanning = false;
  size_t map_entry_count = 0;
  ScanValueType value_type = ScanValueType::kUInt32;
  bool show_all_results = false;
};

}  // namespace maia::gui
