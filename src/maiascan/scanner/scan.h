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
      SetMatches(*matches, sizeof needle);
    }
    return scan_;
  }

  void FilterChanged();

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
