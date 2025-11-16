// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>

#include <memory>
#include <mutex>
#include <thread>

#include "maia/core/i_memory_scanner.h"
#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"
#include "memory_scanner.h"

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

  ScanResultModel() {}

  Signals& signals() {
    return signals_;
  }

  const std::vector<ScanEntry>& entries() const {
    return entries_;
  }

  // void ScanForValue(ScanParams params);

  void FirstScan(std::vector<std::byte> value_to_scan);

  void FilterChangedValues();

  void SetActiveProcess(std::unique_ptr<IProcess> process);

  void Clear();

 private:
  Signals signals_;

  // IProcess* active_process_ = nullptr;
  std::unique_ptr<MemoryScanner> memory_scanner_;

  std::vector<ScanEntry> entries_;
  std::vector<ScanEntry> prev_entries_;

  std::mutex mutex_;
  std::jthread task_;
};

}  // namespace maia
