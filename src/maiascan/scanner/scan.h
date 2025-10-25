#pragma once

#include <algorithm>
#include <memory>

#include "maiascan/scanner/process.h"

namespace maia::scanner {

class Scan {
 public:
  struct Match {
    MemoryAddress address;
    std::vector<Byte> bytes;
  };

  explicit Scan(std::shared_ptr<Process> process)
      : process_(std::move(process)) {}

  template <typename T>
  const std::vector<Match>& Find(T needle) {
    PushScan();
    auto needle_view = std::span<Byte>(
        std::bit_cast<Byte*>(std::addressof(needle)), sizeof needle);
    if (auto matches = process_->Find(needle_view)) {
      // TODO(marco): This function seems kinda odd, verify if it can be made
      // better.
      UpdateScan(*matches, sizeof(needle));
    }
    return scan_;
  }

  void FilterChanged();

  template <CFundamentalType T>
  void RemoveDifferent(T original_value) {
    std::vector<Match> same_matches;
    same_matches.reserve(scan_.size());
    for (const auto& scan : scan_) {
      if (BytesToFundamentalType<T>(scan.bytes) == original_value) {
        same_matches.emplace_back(scan);
      }
    }
    prev_scan_ = std::move(scan_);
    scan_ = std::move(same_matches);
  }

  template <CFundamentalType T>
  void Narrow(T needle) {
    if (scan_.empty()) {
      return;
    }
    std::vector<Match> new_scan;
    new_scan.reserve(scan_.size());

    auto needle_view = ToBytesView(needle);
    T buffer{};
    auto buffer_view = ToBytesView(buffer);
    for (auto& scan_entry : scan_) {
      bool is_match =
          process_->ReadIntoBuffer(scan_entry.address, buffer_view) &&
          std::ranges::equal(buffer_view, scan_entry.bytes);
      if (is_match) {
        new_scan.emplace_back(scan_entry);
      }
    }
    prev_scan_ = std::move(scan_);
    scan_ = std::move(new_scan);
  }

  std::vector<Match>& scan() {
    return scan_;
  }

 private:
  void UpdateScan(const Matches& matches, int buffer_size);

  void PushScan() {
    scan_.swap(prev_scan_);
    scan_.clear();
  }

  std::shared_ptr<Process> process_;
  std::vector<Match> scan_;
  std::vector<Match> prev_scan_;
};

}  // namespace maia::scanner
