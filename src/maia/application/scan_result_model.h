// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>

#include "maia/core/i_memory_scanner.h"
#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"
#include "maia/logging.h"

namespace maia {

struct ScanEntry {
  MemoryAddress address;
  std::vector<std::byte> data;
};

class ScanResultModel {
 public:
  struct Signals {
    entt::sigh<void(std::vector<ScanEntry>)> memory_changed;
  };

  ScanResultModel();

  Signals& signals() {
    return signals_;
  }

  void SetMemory(std::vector<ScanEntry> entries) {
    entries_ = entries;
    signals_.memory_changed.publish(std::move(entries));
  }

  void FirstScan() {
    LogInfo("First scan");
  };

  const std::vector<ScanEntry>& entries() const {
    return entries_;
  }

  void ScanForValue(std::vector<std::byte> value_to_scan);

  void FilterChangedValues();

  void SetActiveProcess(IProcess* process);

 private:
  Signals signals_;

  IProcess* active_process_ = nullptr;
  std::unique_ptr<IMemoryScanner> memory_scanner_;

  std::vector<ScanEntry> entries_;
  std::vector<ScanEntry> prev_entries_;

  std::mutex mutex_;
  std::jthread task_;
};

}  // namespace maia
