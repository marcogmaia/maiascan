// Copyright (c) Maia

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "maia/core/i_process.h"
#include "maia/core/scan_types.h"

namespace maia {

struct CheatTableEntryData {
  mutable std::mutex mutex;  // Protects value, frozen_value, is_frozen
  std::vector<std::byte> value;
  std::vector<std::byte> frozen_value;
  bool is_frozen{false};
};

struct CheatTableEntry {
  MemoryAddress address;
  ScanValueType type;
  std::string description;
  std::shared_ptr<CheatTableEntryData> data;
};

class CheatTableModel {
 public:
  CheatTableModel();
  ~CheatTableModel();

  auto sinks() {
    return Sinks{*this};
  }

  std::shared_ptr<const std::vector<CheatTableEntry>> entries() const;

  void AddEntry(MemoryAddress address,
                ScanValueType type,
                const std::string& description);
  void RemoveEntry(size_t index);
  void UpdateEntryDescription(size_t index, const std::string& description);
  void ToggleFreeze(size_t index);
  void SetValue(size_t index, const std::string& value_str);

  // Called by Presenter/Main loop or internal timer
  void UpdateValues();
  void SetActiveProcess(IProcess* process);

 private:
  struct Signals {
    entt::sigh<void()> table_changed;
  };

  struct Sinks {
    CheatTableModel& model;

    auto TableChanged() {
      return entt::sink(model.signals_.table_changed);
    }
  };

  void AutoUpdateLoop(std::stop_token stop_token);
  void WriteMemory(size_t index, const std::vector<std::byte>& data);

  Signals signals_;
  std::atomic<std::shared_ptr<const std::vector<CheatTableEntry>>> entries_;
  IProcess* active_process_{nullptr};
  mutable std::mutex mutex_;
  std::jthread update_task_;
};

}  // namespace maia
