// Copyright (c) Maia

/// \file cheat_table_model.h
/// \brief Manages the persistent state of user-created cheat entries.
///
/// \details
/// **Role**: The persistent data store for addresses the user wants to keep
/// track of. Handles freezing (locking) values and saving/loading tables.
///
/// **Architecture**:
///    - **Copy-On-Write (COW)**: The list of entries is stored in a
///    `std::atomic<std::shared_ptr>>`.
///      This allows the UI to render a snapshot while the background thread
///      updates values without lock contention or race conditions.
///    - **Background Worker**: Runs a dedicated thread to periodically refresh
///    values
///      from the process and re-apply "frozen" values.
///
/// **Thread Safety**:
///    - Thread-safe by design using COW and internal mutexes for individual
///    entry data.
///
/// **Key Interactions**:
///    - Consumed by `CheatTablePresenter`.
///    - Listens to `ProcessModel`.

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <entt/signal/sigh.hpp>
#include <nlohmann/json.hpp>

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

  /// \brief Returns a copy of the previous value (before the last update).
  std::vector<std::byte> GetPrevValue() const;

  /// \brief Updates the internal value from a process read result.
  /// \details Performs a comparison and only updates if the value has changed.
  void UpdateFromProcess(const std::span<const std::byte>& new_value);

  /// \brief Returns the time point when the value last changed.
  std::chrono::steady_clock::time_point GetLastChangeTime() const;

  /// \brief Returns the last resolved address for this entry.
  MemoryAddress GetResolvedAddress() const;

  /// \brief Sets the last resolved address for this entry.
  void SetResolvedAddress(MemoryAddress address);

 private:
  mutable std::mutex mutex_;
  std::vector<std::byte> value_;
  std::vector<std::byte> prev_value_;
  std::vector<std::byte> frozen_value_;
  bool is_frozen_{false};
  std::chrono::steady_clock::time_point last_change_time_;
  MemoryAddress resolved_address_{0};
};

/// \brief Represents an entry in the cheat table.
struct CheatTableEntry {
  // For static addresses: direct memory address
  MemoryAddress address;

  // For dynamic addresses: base address of the chain (e.g., module base +
  // offset)
  MemoryAddress pointer_base = 0;

  // For dynamic addresses: module name if base is relative to a module
  std::string pointer_module;

  // For dynamic addresses: offset from module base to the base address
  uint64_t pointer_module_offset = 0;

  // For dynamic addresses: offsets to follow from base to final address
  // Example: [0x10, 0x48] means [[base]+0x10]+0x48
  std::vector<int64_t> pointer_offsets;

  // Type and metadata
  ScanValueType type;
  std::string description;
  bool show_as_hex = false;
  std::shared_ptr<CheatTableEntryData> data;

  /// \brief Check if this entry requires dynamic resolution.
  [[nodiscard]] bool IsDynamicAddress() const {
    return !pointer_module.empty() || pointer_base != 0 ||
           !pointer_offsets.empty();
  }
};

/// \brief Manages the list of cheat table entries and handles auto-updates.
/// \details This class maintains the source of truth for the cheat table.
/// It spawns a background thread that periodically reads values from the
/// active process and re-applies frozen values.
class CheatTableModel {
 public:
  explicit CheatTableModel(std::unique_ptr<core::ITaskRunner> task_runner =
                               std::make_unique<core::AsyncTaskRunner>());

  ~CheatTableModel();

  auto sinks() {
    return Sinks{*this};
  }

  /// \brief Returns a shared pointer to the current snapshot of entries.
  std::shared_ptr<const std::vector<CheatTableEntry>> entries() const;

  /// \brief Adds a new entry to the cheat table (static address).
  /// \param address The memory address to track.
  /// \param type The data type of the value at the address.
  /// \param description A user-provided name for the entry.
  /// \param size (Optional) Override for the data size.
  void AddEntry(MemoryAddress address,
                ScanValueType type,
                const std::string& description,
                size_t size = 0);

  /// \brief Adds a new pointer chain entry to the cheat table.
  /// \param base_address The static base address (e.g., module + offset).
  /// \param offsets The chain of offsets to follow from base.
  /// \param module_name Module name if base is relative (empty for absolute).
  /// \param module_offset Offset from module base to the pointer.
  /// \param type The data type of the final value.
  /// \param description A user-provided name for the entry.
  /// \param size (Optional) Override for the data size.
  void AddPointerChainEntry(MemoryAddress base_address,
                            const std::vector<int64_t>& offsets,
                            const std::string& module_name,
                            uint64_t module_offset,
                            ScanValueType type,
                            const std::string& description,
                            size_t size = 0);

  /// \brief Removes an entry from the cheat table by its index.
  void RemoveEntry(size_t index);

  /// \brief Updates the description of an entry.
  void UpdateEntryDescription(size_t index, const std::string& description);

  /// \brief Toggles the hex display mode of an entry.
  void SetShowAsHex(size_t index, bool show_as_hex);

  /// \brief Changes the type of an entry and resizes its data buffer.
  void ChangeEntryType(size_t index, ScanValueType new_type);

  /// \brief Toggles the frozen state of an entry.
  void ToggleFreeze(size_t index);

  /// \brief Manually sets the value of an entry and writes it to the process.
  void SetValue(size_t index, const std::string& value_str);

  /// \brief Saves the cheat table to a file.
  bool Save(const std::filesystem::path& path) const;

  /// \brief Loads the cheat table from a file.
  bool Load(const std::filesystem::path& path);

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

  /// \brief Resolves a dynamic entry to get the final memory address.
  /// \param entry The cheat table entry containing the address info.
  /// \return The resolved address, or 0 if resolution failed.
  [[nodiscard]] MemoryAddress ResolveAddress(
      const CheatTableEntry& entry) const;

  /// \brief Reads memory for an entry, handling both static and dynamic.
  [[nodiscard]] bool ReadEntryValue(const CheatTableEntry& entry,
                                    std::span<std::byte> out_buffer);

  /// \brief Writes memory for an entry, handling both static and dynamic.
  [[nodiscard]] bool WriteEntryValue(const CheatTableEntry& entry,
                                     std::span<const std::byte> data);

  Signals signals_;
  std::atomic<std::shared_ptr<const std::vector<CheatTableEntry>>> entries_;
  IProcess* active_process_{nullptr};
  mutable std::mutex mutex_;
  std::unique_ptr<core::ITaskRunner> task_runner_;
};

}  // namespace maia
