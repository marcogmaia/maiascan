#pragma once

#include <algorithm>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"

namespace maia::scanner {

class Scan {
 public:
  struct ScanMatch {
    Bytes bytes;
    MemoryAddress address;
  };

  explicit Scan(Process* process) : process_(*process) {}

  template <typename T>
  const std::vector<ScanMatch>& Find(T needle) {
    SwapScans();
    if (auto matches = process_.Find(ToBytesView(needle))) {
      SetMatches(*matches, sizeof needle);
    }
    return scan_;
  }

  void FilterChanged() {
    bool can_filter = !scan_.empty() && !prev_scan_.empty() && scan_.size() == prev_scan_.size();
    if (!can_filter) {
      return;
    }
    SwapScans();
    std::vector<ScanMatch> scan_changed;
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

  std::vector<ScanMatch>& scan() { return scan_; }

 private:
  void SetMatches(const Matches& matches, int buffer_size) {
    ForEachMatchesAddress(matches, [this, buffer_size](MemoryAddress address) {
      Bytes buffer(buffer_size, std::byte{});
      if (process_.ReadIntoBuffer(address, buffer)) {
        scan_.emplace_back(ScanMatch{buffer, address});
      }
    });
  }

  void SwapScans() {
    scan_.swap(prev_scan_);
    scan_.clear();
  }

  Process& process_;
  std::vector<ScanMatch> scan_{};
  std::vector<ScanMatch> prev_scan_{};
};

}  // namespace maia::scanner
