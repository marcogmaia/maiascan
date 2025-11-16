// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>

#include <mutex>
#include <thread>

#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"

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

  // void FilterChangedValues();

  void SetActiveProcess(IProcess* process);

  void Clear();

 private:
  Signals signals_;

  // IProcess* active_process_ = nullptr;
  IProcess* active_process_{};

  std::vector<ScanEntry> entries_;
  std::vector<ScanEntry> prev_entries_;

  std::mutex mutex_;
  std::jthread task_;
};

}  // namespace maia
