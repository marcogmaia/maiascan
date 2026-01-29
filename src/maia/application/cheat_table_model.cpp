// Copyright (c) Maia

#include "maia/application/cheat_table_model.h"

#include <array>
#include <cstring>

#include "maia/assert.h"
#include "maia/core/memory_common.h"
#include "maia/core/value_parser.h"
#include "maia/logging.h"

namespace maia {

void CheatTableEntryData::Resize(size_t size) {
  std::scoped_lock lock(mutex_);
  value_.resize(size);
  frozen_value_.resize(size);
}

std::vector<std::byte> CheatTableEntryData::GetValue() const {
  std::scoped_lock lock(mutex_);
  return value_;
}

size_t CheatTableEntryData::GetValueSize() const {
  std::scoped_lock lock(mutex_);
  return value_.size();
}

void CheatTableEntryData::SetValue(const std::vector<std::byte>& new_value) {
  std::scoped_lock lock(mutex_);
  value_ = new_value;
  if (is_frozen_) {
    frozen_value_ = new_value;
  }
}

bool CheatTableEntryData::IsFrozen() const {
  std::scoped_lock lock(mutex_);
  return is_frozen_;
}

void CheatTableEntryData::ToggleFreeze() {
  std::scoped_lock lock(mutex_);
  is_frozen_ = !is_frozen_;
  if (is_frozen_) {
    frozen_value_ = value_;
  }
}

std::vector<std::byte> CheatTableEntryData::GetFrozenValue() const {
  std::scoped_lock lock(mutex_);
  return frozen_value_;
}

void CheatTableEntryData::UpdateFromProcess(
    const std::span<const std::byte>& new_value) {
  std::scoped_lock lock(mutex_);
  const size_t size_to_copy = std::min(value_.size(), new_value.size());
  if (size_to_copy == 0) {
    return;
  }
  if (std::memcmp(value_.data(), new_value.data(), size_to_copy) != 0) {
    std::memcpy(value_.data(), new_value.data(), size_to_copy);
  }
}

CheatTableModel::CheatTableModel(std::unique_ptr<core::ITaskRunner> task_runner)
    : entries_(std::make_shared<std::vector<CheatTableEntry>>()),
      task_runner_(std::move(task_runner)) {
  Assert(task_runner_ != nullptr);
  task_runner_->Run(
      [this](std::stop_token stop_token) { AutoUpdateLoop(stop_token); });
}

CheatTableModel::~CheatTableModel() {
  task_runner_->RequestStop();
  task_runner_->Join();
}

std::shared_ptr<const std::vector<CheatTableEntry>> CheatTableModel::entries()
    const {
  return entries_.load();
}

void CheatTableModel::AddEntry(MemoryAddress address,
                               ScanValueType type,
                               const std::string& description,
                               size_t size) {
  std::scoped_lock lock(mutex_);

  auto current_snapshot = entries_.load();
  auto new_entries =
      std::make_shared<std::vector<CheatTableEntry>>(*current_snapshot);

  CheatTableEntry entry;
  entry.address = address;
  entry.type = type;
  entry.description = description;
  entry.data = std::make_shared<CheatTableEntryData>();

  const size_t entry_size = size > 0 ? size : GetSizeForType(type);
  entry.data->Resize(entry_size);

  // Initial read
  if (active_process_) {
    auto initial_value = entry.data->GetValue();
    active_process_->ReadMemory(
        {&entry.address, 1}, initial_value.size(), initial_value);
    entry.data->UpdateFromProcess(initial_value);
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
    (*snapshot)[index].data->ToggleFreeze();
  }
}

void CheatTableModel::SetValue(size_t index, const std::string& value_str) {
  auto snapshot = entries_.load();
  if (index < snapshot->size()) {
    const auto& entry = (*snapshot)[index];
    auto data = ParseStringByType(value_str, entry.type);
    if (data.empty() && !value_str.empty() &&
        entry.type != ScanValueType::kString) {
      return;
    }

    // Enforce size for strings and byte arrays to avoid OOB writes in the
    // target process.
    if (entry.type == ScanValueType::kString ||
        entry.type == ScanValueType::kWString ||
        entry.type == ScanValueType::kArrayOfBytes) {
      const size_t original_size = entry.data->GetValueSize();
      if (data.size() > original_size) {
        data.resize(original_size);
      } else if (data.size() < original_size) {
        // Pad with nulls
        data.resize(original_size, std::byte{0});
      }
    }

    // Write to process (if valid)
    WriteMemory(index, data);

    // Update local version
    entry.data->SetValue(data);
  }
}

void CheatTableModel::SetActiveProcess(IProcess* process) {
  std::scoped_lock lock(mutex_);
  active_process_ = process;
}

void CheatTableModel::WriteMemory(size_t index,
                                  const std::vector<std::byte>& data) {
  // Capture pointer to avoid race with UpdateValues nulling it
  if (IProcess* proc = active_process_; proc->IsProcessValid()) {
    auto snapshot = entries_.load();
    if (index < snapshot->size()) {
      proc->WriteMemory((*snapshot)[index].address, data);
    }
  }
}

void CheatTableModel::UpdateValues() {
  auto snapshot = entries_.load();

  if (!active_process_) {
    return;
  }

  if (!active_process_->IsProcessValid()) {
    LogInfo("Active process is no longer valid. Clearing.");
    std::scoped_lock lock(mutex_);
    active_process_ = nullptr;
    // We can't easily zero out values in a const snapshot without casting,
    // but the UI will handle active_process_ == nullptr.
    signals_.table_changed.publish();
    return;
  }

  for (const auto& entry : *snapshot) {
    if (entry.data->IsFrozen()) {
      auto frozen_val = entry.data->GetFrozenValue();
      if (active_process_->WriteMemory(entry.address, frozen_val)) {
        entry.data->UpdateFromProcess(frozen_val);
      } else {
        LogWarning("Failed to write frozen value to 0x{:X}", entry.address);
      }
    } else {
      // TODO: Review this stack allocation.
      // Use the actual size of the entry's value buffer.
      std::array<std::byte, 1024> stack_buffer;
      const size_t entry_size = entry.data->GetValueSize();
      const size_t read_size = std::min(entry_size, stack_buffer.size());

      if (read_size > 0 &&
          active_process_->ReadMemory({&entry.address, 1},
                                      read_size,
                                      {stack_buffer.data(), read_size})) {
        entry.data->UpdateFromProcess({stack_buffer.data(), read_size});
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
