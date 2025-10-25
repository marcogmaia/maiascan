// Copyright (c) Maia

#include "maiascan/scanner/scan.h"

#include <vector>

#include <maiascan/scanner/engine.h>

namespace maia::scanner {

void Scan::FilterChanged() {
  bool can_filter = !scan_.empty() && !prev_scan_.empty() &&
                    scan_.size() == prev_scan_.size();
  if (!can_filter) {
    return;
  }
  PushScan();
  std::vector<Match> scan_changed;
  scan_changed.reserve(std::max(scan_.size(), prev_scan_.size()));
  for (auto it_actual = scan_.begin(), it_prev = prev_scan_.begin();
       it_actual < scan_.end() && it_prev < prev_scan_.end();
       std::advance(it_actual, 1), std::advance(it_prev, 1)) {
    bool changed = !std::ranges::equal(it_actual->bytes, it_prev->bytes);
    if (changed) {
      scan_changed.emplace_back(*it_actual);
    }
  }
  scan_changed.swap(scan_);
}

void Scan::UpdateScan(const Matches& matches, int buffer_size) {
  ForEachMatchAddress(matches, [this, buffer_size](MemoryAddress address) {
    std::vector<Byte> buffer(buffer_size);
    if (process_->ReadIntoBuffer(address, buffer)) {
      scan_.emplace_back(Match{.address = address, .bytes = buffer});
    }
  });
}

}  // namespace maia::scanner
