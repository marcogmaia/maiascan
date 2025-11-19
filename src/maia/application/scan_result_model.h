// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>

#include <mutex>
#include <thread>

#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"
#include "maia/core/scan_types.h"

namespace maia {

struct ScanEntry {
  MemoryAddress address;
  std::vector<std::byte> data;
};

// class Snapshot {}

class ScanResultModel {
 public:
  struct Signals {
    entt::sigh<void(std::vector<ScanEntry>)> memory_changed;
  };

  // ScanResultModel() = default;

  Signals& signals() {
    return signals_;
  }

  const std::vector<ScanEntry>& entries() const {
    return entries_;
  }

  // void ScanForValue(ScanParams params);

  void FirstScan(std::vector<std::byte> value_to_scan);

  // void ScanForValue(std::vector<std::byte> value_to_scan) {}

  // TODO: Implement here the next scan for changed, increased and decreased,
  // this function queries the actual entries addresses and then filters the
  // addresses accordingly to the values it points to.
  void NextScan();

  // void FilterChangedValues();

  void SetActiveProcess(IProcess* process);

  void SetScanComparison(ScanComparison scan_comparison) {
    scan_comparison_ = scan_comparison;
  }

  void Clear();

  /// \brief Starts a background thread that refreshes values every second.
  void StartAutoUpdate();

  /// \brief Stops the background update thread.
  void StopAutoUpdate();

 private:
  /// \brief Helper to refresh the data of current entries without filtering.
  void UpdateCurrentValues();

  Signals signals_;

  // IProcess* active_process_ = nullptr;
  IProcess* active_process_{};

  ScanComparison scan_comparison_{ScanComparison::kChanged};

  std::vector<ScanEntry> entries_;
  std::vector<ScanEntry> prev_entries_;

  std::mutex mutex_;
  std::jthread task_;
};

}  // namespace maia
