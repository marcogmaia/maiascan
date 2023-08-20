#pragma once

#include <algorithm>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"

namespace maia::scanner {

class Scan {
 public:
  struct ScanMatch {
    MemoryAddress address;
    Bytes bytes;
  };

  explicit Scan(Process* process) : process_(*process) {}

  template <typename T>
  const std::vector<ScanMatch>& Find(T needle) {
    SwapScans();
    if (auto matches = process_.Find(ToBytesView(needle))) {
      // TODO(marco): This function seems kinda odd, verify if it can be made better.
      SetMatches(*matches, sizeof needle);
    }
    return scan_;
  }

  void FilterChanged();

  template <CFundamentalType T>
  void Narrow(T needle) {
    if (scan_.empty()) {
      return;
    }
    std::vector<ScanMatch> new_scan;
    new_scan.reserve(scan_.size());

    auto needle_view = ToBytesView(needle);
    T buffer{};
    auto buffer_view = ToBytesView(buffer);
    for (auto& scan_entry : scan_) {
      process_.ReadIntoBuffer(scan_entry.address, buffer_view);
      bool is_match = std::ranges::equal(buffer_view, scan_entry.bytes);
      if (is_match) {
        new_scan.emplace_back(scan_entry);
      }
    }
    prev_scan_ = std::move(scan_);
    scan_ = std::move(new_scan);
  }

  std::vector<ScanMatch>& scan() { return scan_; }

 private:
  void SetMatches(const Matches& matches, int buffer_size);

  void SwapScans() {
    scan_.swap(prev_scan_);
    scan_.clear();
  }

  Process& process_;
  std::vector<ScanMatch> scan_{};
  std::vector<ScanMatch> prev_scan_{};
};

}  // namespace maia::scanner
