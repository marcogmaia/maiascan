// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>

#include "maia/core/i_memory_scanner.h"
#include "maia/core/memory_common.h"
#include "maia/logging.h"

namespace maia {

struct ScanEntry {
  MemoryAddress address;
  std::vector<std::byte> data;
  // size_t size;
};

class ScanResultModel {
 public:
  struct Signals {
    entt::sigh<void(std::vector<ScanEntry>)> memory_changed;
  };

  // explicit ScanResultModel(IMemoryScanner* memory_scanner)
  //     {}

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

 private:
  Signals signals_;
  // IMemoryScanner& memory_scanner_;
  std::vector<ScanEntry> entries_;
};

}  // namespace maia
