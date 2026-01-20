// Copyright (c) Maia

#include "maia/application/cheat_table_model.h"

#include "maia/core/memory_common.h"
#include "maia/core/value_parser.h"

namespace maia {

CheatTableModel::CheatTableModel() {
  update_task_ = std::jthread(
      [this](std::stop_token stop_token) { AutoUpdateLoop(stop_token); });
}

CheatTableModel::~CheatTableModel() {
  update_task_.request_stop();
}

void CheatTableModel::AddEntry(MemoryAddress address,
                               ScanValueType type,
                               const std::string& description) {
  std::scoped_lock lock(mutex_);
  CheatTableEntry entry;
  entry.address = address;
  entry.type = type;
  entry.description = description;
  entry.value.resize(GetSizeForType(type));
  entry.frozen_value.resize(GetSizeForType(type));
  entries_.push_back(std::move(entry));

  // Initial read
  if (active_process_) {
    active_process_->ReadMemory({&entries_.back().address, 1},
                                entries_.back().value.size(),
                                entries_.back().value);
  }

  signals_.table_changed.publish();
}

void CheatTableModel::RemoveEntry(size_t index) {
  std::scoped_lock lock(mutex_);
  if (index < entries_.size()) {
    entries_.erase(entries_.begin() + index);
    signals_.table_changed.publish();
  }
}

void CheatTableModel::UpdateEntryDescription(size_t index,
                                             const std::string& description) {
  std::scoped_lock lock(mutex_);
  if (index < entries_.size()) {
    entries_[index].description = description;
  }
}

void CheatTableModel::ToggleFreeze(size_t index) {
  std::scoped_lock lock(mutex_);
  if (index < entries_.size()) {
    entries_[index].is_frozen = !entries_[index].is_frozen;
    if (entries_[index].is_frozen) {
      // When freezing, capture current value as frozen value
      // Or should we use the last known value? Let's use current value from
      // memory if possible. For now, use the cached value.
      entries_[index].frozen_value = entries_[index].value;
    }
  }
}

void CheatTableModel::SetValue(size_t index, const std::string& value_str) {
  std::scoped_lock lock(mutex_);
  if (index < entries_.size()) {
    auto data = ParseStringByType(value_str, entries_[index].type);
    if (data.empty()) {
      return;
    }

    if (active_process_) {
      WriteMemory(index, data);
    }

    // Update local value immediately for responsiveness
    entries_[index].value = data;
    if (entries_[index].is_frozen) {
      entries_[index].frozen_value = data;
    }
  }
}

void CheatTableModel::SetActiveProcess(IProcess* process) {
  std::scoped_lock lock(mutex_);
  active_process_ = process;
}

void CheatTableModel::WriteMemory(size_t index,
                                  const std::vector<std::byte>& data) {
  if (active_process_ && active_process_->IsProcessValid()) {
    active_process_->WriteMemory(entries_[index].address, data);
  }
}

void CheatTableModel::UpdateValues() {
  std::scoped_lock lock(mutex_);
  if (!active_process_ || !active_process_->IsProcessValid()) {
    return;
  }

  for (auto& entry : entries_) {
    if (entry.is_frozen) {
      // Write frozen value
      active_process_->WriteMemory(entry.address, entry.frozen_value);
      // And also ensure displayed value matches frozen value
      entry.value = entry.frozen_value;
    } else {
      // Read current value
      active_process_->ReadMemory(
          {&entry.address, 1}, entry.value.size(), entry.value);
    }
  }
}

void CheatTableModel::AutoUpdateLoop(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    UpdateValues();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

}  // namespace maia
