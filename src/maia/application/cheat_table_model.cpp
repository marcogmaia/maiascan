// Copyright (c) Maia

#include "maia/application/cheat_table_model.h"

#include <array>
#include <cstring>

#include "maia/core/memory_common.h"
#include "maia/core/value_parser.h"

namespace maia {

CheatTableModel::CheatTableModel()
    : entries_(std::make_shared<std::vector<CheatTableEntry>>()) {
  update_task_ = std::jthread(
      [this](std::stop_token stop_token) { AutoUpdateLoop(stop_token); });
}

CheatTableModel::~CheatTableModel() {
  update_task_.request_stop();
}

std::shared_ptr<const std::vector<CheatTableEntry>> CheatTableModel::entries()
    const {
  return entries_.load();
}

void CheatTableModel::AddEntry(MemoryAddress address,
                               ScanValueType type,
                               const std::string& description) {
  std::scoped_lock lock(mutex_);

  auto current_snapshot = entries_.load();
  auto new_entries =
      std::make_shared<std::vector<CheatTableEntry>>(*current_snapshot);

  CheatTableEntry entry;
  entry.address = address;
  entry.type = type;
  entry.description = description;
  entry.data = std::make_shared<CheatTableEntryData>();
  entry.data->value.resize(GetSizeForType(type));
  entry.data->frozen_value.resize(GetSizeForType(type));

  // Initial read
  if (active_process_) {
    active_process_->ReadMemory(
        {&entry.address, 1}, entry.data->value.size(), entry.data->value);
  }

  new_entries->emplace_back(std::move(entry));
  entries_.store(std::move(new_entries));

  signals_.table_changed.publish();
}

void CheatTableModel::RemoveEntry(size_t index) {
  std::scoped_lock lock(mutex_);
  auto current_snapshot = entries_.load();
  if (index < current_snapshot->size()) {
    auto new_entries =
        std::make_shared<std::vector<CheatTableEntry>>(*current_snapshot);
    new_entries->erase(new_entries->begin() + index);
    entries_.store(std::move(new_entries));
    signals_.table_changed.publish();
  }
}

void CheatTableModel::UpdateEntryDescription(size_t index,
                                             const std::string& description) {
  std::scoped_lock lock(mutex_);
  auto current_snapshot = entries_.load();
  if (index < current_snapshot->size()) {
    auto new_entries =
        std::make_shared<std::vector<CheatTableEntry>>(*current_snapshot);
    (*new_entries)[index].description = description;
    entries_.store(std::move(new_entries));
  }
}

void CheatTableModel::ToggleFreeze(size_t index) {
  auto snapshot = entries_.load();
  if (index < snapshot->size()) {
    auto& entry = (*snapshot)[index];
    std::scoped_lock entry_lock(entry.data->mutex);
    entry.data->is_frozen = !entry.data->is_frozen;
    if (entry.data->is_frozen) {
      entry.data->frozen_value = entry.data->value;
    }
  }
}

void CheatTableModel::SetValue(size_t index, const std::string& value_str) {
  auto snapshot = entries_.load();
  if (index < snapshot->size()) {
    auto& entry = (*snapshot)[index];
    auto data = ParseStringByType(value_str, entry.type);
    if (data.empty()) {
      return;
    }

    if (active_process_) {
      active_process_->WriteMemory(entry.address, data);
    }

    // Update local version
    std::scoped_lock entry_lock(entry.data->mutex);
    entry.data->value = data;
    if (entry.data->is_frozen) {
      entry.data->frozen_value = data;
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
    auto snapshot = entries_.load();
    if (index < snapshot->size()) {
      active_process_->WriteMemory((*snapshot)[index].address, data);
    }
  }
}

void CheatTableModel::UpdateValues() {
  auto snapshot = entries_.load();

  if (!active_process_) {
    return;
  }

  if (!active_process_->IsProcessValid()) {
    std::scoped_lock lock(mutex_);
    active_process_ = nullptr;
    // We can't easily zero out values in a const snapshot without casting,
    // but the UI will handle active_process_ == nullptr.
    signals_.table_changed.publish();
    return;
  }

  for (auto& entry : *snapshot) {
    if (entry.data->is_frozen) {
      std::scoped_lock entry_lock(entry.data->mutex);
      active_process_->WriteMemory(entry.address, entry.data->frozen_value);
      entry.data->value = entry.data->frozen_value;
    } else {
      // Small optimization: use a stack buffer for small types to avoid heap
      // allocation in the loop.
      std::array<std::byte, 64> stack_buffer;
      const size_t type_size = GetSizeForType(entry.type);
      const size_t read_size = std::min(type_size, stack_buffer.size());

      if (active_process_->ReadMemory({&entry.address, 1},
                                      read_size,
                                      {stack_buffer.data(), read_size})) {
        std::scoped_lock entry_lock(entry.data->mutex);
        if (std::memcmp(entry.data->value.data(),
                        stack_buffer.data(),
                        read_size) != 0) {
          std::memcpy(entry.data->value.data(), stack_buffer.data(), read_size);
        }
      }
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
