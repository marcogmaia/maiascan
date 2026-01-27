// Copyright (c) Maia

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "maia/core/i_process.h"
#include "maia/core/scan_types.h"
#include "maia/core/task_runner.h"

namespace maia {

/// \brief Thread-safe container for a single cheat table entry's dynamic data.
/// \details Encapsulates value storage and freeze state, protecting them with
/// an internal mutex to ensure safe concurrent access from the UI thread and
/// the background update loop.
class CheatTableEntryData {
 public:
  CheatTableEntryData() = default;

  /// \brief Resizes the internal value and frozen value buffers.
  void Resize(size_t size);

  /// \brief Returns a copy of the current value.
  std::vector<std::byte> GetValue() const;

  /// \brief Returns the size of the internal value buffer.
  size_t GetValueSize() const;

  /// \brief Sets the current value and updates the frozen value if the entry is
  /// frozen.
  void SetValue(const std::vector<std::byte>& new_value);

  /// \brief Checks if the entry is currently frozen.
  bool IsFrozen() const;

  /// \brief Toggles the frozen state of the entry.
  void ToggleFreeze();

  /// \brief Returns a copy of the value to be held when frozen.
  std::vector<std::byte> GetFrozenValue() const;

  /// \brief Updates the internal value from a process read result.
  /// \details Performs a comparison and only updates if the value has changed.
  void UpdateFromProcess(const std::span<const std::byte>& new_value);

 private:
  mutable std::mutex mutex_;
  std::vector<std::byte> value_;
  std::vector<std::byte> frozen_value_;
  bool is_frozen_{false};
};

/// \brief Represents an entry in the cheat table.
struct CheatTableEntry {
  MemoryAddress address;
  ScanValueType type;
  std::string description;
  std::shared_ptr<CheatTableEntryData> data;
};

/// \brief Manages the list of cheat table entries and handles auto-updates.
/// \details This class maintains the source of truth for the cheat table.
/// It spawns a background thread that periodically reads values from the
/// active process and re-applies frozen values.
class CheatTableModel {
 public:
  explicit CheatTableModel(
      std::unique_ptr<core::ITaskRunner> task_runner = nullptr);

  ~CheatTableModel();

  auto sinks() {
    return Sinks{*this};
  }

  /// \brief Returns a shared pointer to the current snapshot of entries.
  std::shared_ptr<const std::vector<CheatTableEntry>> entries() const;

  /// \brief Adds a new entry to the cheat table.
  /// \param address The memory address to track.
  /// \param type The data type of the value at the address.
  /// \param description A user-provided name for the entry.
  /// \param size (Optional) Override for the data size.
  void AddEntry(MemoryAddress address,
                ScanValueType type,
                const std::string& description,
                size_t size = 0);

  /// \brief Removes an entry from the cheat table by its index.
  void RemoveEntry(size_t index);

  /// \brief Updates the description of an entry.
  void UpdateEntryDescription(size_t index, const std::string& description);

  /// \brief Toggles the frozen state of an entry.
  void ToggleFreeze(size_t index);

  // TODO: Refactor this interface to accept numbers instead of strings? Well,
  // strings are flexible enough, think a little bit.

  /// \brief Manually sets the value of an entry and writes it to the process.
  void SetValue(size_t index, const std::string& value_str);

  /// \brief Periodically called to synchronize values with the target process.
  /// \details Reads current values and reapplies frozen values.
  void UpdateValues();

  /// \brief Sets the target process for memory operations.
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
  std::unique_ptr<core::ITaskRunner> task_runner_;
};

}  // namespace maia
