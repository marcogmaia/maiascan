// Copyright (c) Maia

#include "maia/application/cheat_table_model.h"

#include <array>
#include <bit>
#include <cstring>
#include <fstream>

#include "maia/assert.h"
#include "maia/core/memory_common.h"
#include "maia/core/value_parser.h"
#include "maia/logging.h"

namespace maia {

// clang-format off
// JSON serialization for ScanValueType
NLOHMANN_JSON_SERIALIZE_ENUM(ScanValueType,
                                {{         maia::ScanValueType::kInt8,         "Int8"},
                                 {        maia::ScanValueType::kUInt8,        "UInt8"},
                                 {        maia::ScanValueType::kInt16,        "Int16"},
                                 {       maia::ScanValueType::kUInt16,       "UInt16"},
                                 {        maia::ScanValueType::kInt32,        "Int32"},
                                 {       maia::ScanValueType::kUInt32,       "UInt32"},
                                 {        maia::ScanValueType::kInt64,        "Int64"},
                                 {       maia::ScanValueType::kUInt64,       "UInt64"},
                                 {        maia::ScanValueType::kFloat,        "Float"},
                                 {       maia::ScanValueType::kDouble,       "Double"},
                                 {       maia::ScanValueType::kString,       "String"},
                                 {      maia::ScanValueType::kWString,      "WString"},
                                 { maia::ScanValueType::kArrayOfBytes, "ArrayOfBytes"}});
// clang-format on

// JSON serialization for CheatTableEntry (without runtime data member)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CheatTableEntry,
                                   address,
                                   pointer_base,
                                   pointer_module,
                                   pointer_module_offset,
                                   pointer_offsets,
                                   type,
                                   description)

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

std::vector<std::byte> CheatTableEntryData::GetPrevValue() const {
  std::scoped_lock lock(mutex_);
  return prev_value_;
}

void CheatTableEntryData::UpdateFromProcess(
    const std::span<const std::byte>& new_value) {
  std::scoped_lock lock(mutex_);
  const size_t size_to_copy = std::min(value_.size(), new_value.size());
  if (size_to_copy == 0) {
    return;
  }
  if (std::memcmp(value_.data(), new_value.data(), size_to_copy) != 0) {
    // Save current as previous before updating
    prev_value_ = value_;
    std::memcpy(value_.data(), new_value.data(), size_to_copy);
    // Record the time of change for blink effect
    last_change_time_ = std::chrono::steady_clock::now();
  }
}

std::chrono::steady_clock::time_point CheatTableEntryData::GetLastChangeTime()
    const {
  std::scoped_lock lock(mutex_);
  return last_change_time_;
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

void CheatTableModel::AddPointerChainEntry(MemoryAddress base_address,
                                           const std::vector<int64_t>& offsets,
                                           const std::string& module_name,
                                           uint64_t module_offset,
                                           ScanValueType type,
                                           const std::string& description,
                                           size_t size) {
  std::scoped_lock lock(mutex_);

  auto current_snapshot = entries_.load();
  auto new_entries =
      std::make_shared<std::vector<CheatTableEntry>>(*current_snapshot);

  CheatTableEntry entry;
  // For pointer chains, address is 0 (will be resolved dynamically)
  entry.address = 0;
  entry.pointer_base = base_address;
  entry.pointer_offsets = offsets;
  entry.pointer_module = module_name;
  entry.pointer_module_offset = module_offset;
  entry.type = type;
  entry.description = description;
  entry.data = std::make_shared<CheatTableEntryData>();

  const size_t entry_size = size > 0 ? size : GetSizeForType(type);
  entry.data->Resize(entry_size);

  // Initial read - resolve the pointer chain
  if (active_process_) {
    std::vector<std::byte> initial_value(entry_size);
    if (ReadEntryValue(entry, initial_value)) {
      entry.data->UpdateFromProcess(initial_value);
    }
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

bool CheatTableModel::Save(const std::filesystem::path& path) const {
  std::scoped_lock lock(mutex_);
  auto snapshot = entries_.load();

  try {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& entry : *snapshot) {
      nlohmann::json entry_json;
      entry_json["address"] = entry.address;
      entry_json["pointer_base"] = entry.pointer_base;
      entry_json["pointer_module"] = entry.pointer_module;
      entry_json["pointer_module_offset"] = entry.pointer_module_offset;
      entry_json["pointer_offsets"] = entry.pointer_offsets;
      entry_json["type"] = entry.type;
      entry_json["description"] = entry.description;
      j.push_back(entry_json);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
      LogError("Failed to open file for saving: {}", path.string());
      return false;
    }
    file << j.dump(2);
    LogInfo("Saved {} entries to {}", snapshot->size(), path.string());
    return true;
  } catch (const std::exception& e) {
    LogError("Failed to save cheat table: {}", e.what());
    return false;
  }
}

bool CheatTableModel::Load(const std::filesystem::path& path) {
  std::scoped_lock lock(mutex_);

  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      LogWarning("Failed to open file for loading: {}", path.string());
      return false;
    }

    nlohmann::json j;
    file >> j;

    std::vector<CheatTableEntry> entries;
    entries.reserve(j.size());

    for (const auto& entry_json : j) {
      CheatTableEntry entry;
      entry.address = entry_json["address"].get<MemoryAddress>();
      entry.pointer_base = entry_json.value("pointer_base", 0);
      entry.pointer_module = entry_json.value("pointer_module", "");
      entry.pointer_module_offset =
          entry_json.value("pointer_module_offset", 0);

      if (entry_json.contains("pointer_offsets")) {
        entry.pointer_offsets =
            entry_json["pointer_offsets"].get<std::vector<int64_t>>();
      }

      entry.type = entry_json["type"].get<ScanValueType>();
      entry.description = entry_json["description"].get<std::string>();

      entry.data = std::make_shared<CheatTableEntryData>();
      entry.data->Resize(GetSizeForType(entry.type));

      entries.push_back(std::move(entry));
    }

    auto new_entries = std::make_shared<std::vector<CheatTableEntry>>(entries);
    entries_.store(new_entries);
    signals_.table_changed.publish();

    LogInfo("Loaded {} entries from {}", entries.size(), path.string());
    return true;
  } catch (const std::exception& e) {
    LogError("Failed to load cheat table: {}", e.what());
    return false;
  }
}

void CheatTableModel::SetActiveProcess(IProcess* process) {
  std::scoped_lock lock(mutex_);
  active_process_ = process;
}

void CheatTableModel::WriteMemory(size_t index,
                                  const std::vector<std::byte>& data) {
  auto snapshot = entries_.load();
  if (index < snapshot->size()) {
    if (!WriteEntryValue((*snapshot)[index], data)) {
      LogWarning("Failed to write memory for entry {}", index);
    }
  }
}

void CheatTableModel::UpdateValues() {
  auto snapshot = entries_.load();

  if (!snapshot) {
    LogWarning("Snapshot isn't valid.");
    return;
  }

  if (!active_process_) {
    // This is here to avoid excessive logs.
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

  // Reuse a single buffer across all entries to minimize allocations.
  std::vector<std::byte> read_buffer;

  for (const auto& entry : *snapshot) {
    if (entry.data->IsFrozen()) {
      auto frozen_val = entry.data->GetFrozenValue();
      if (WriteEntryValue(entry, frozen_val)) {
        entry.data->UpdateFromProcess(frozen_val);
      } else {
        MemoryAddress addr =
            entry.IsPointerChain() ? ResolvePointerChain(entry) : entry.address;
        LogWarning("Failed to write frozen value to 0x{:X}", addr);
      }
    } else {
      const size_t entry_size = entry.data->GetValueSize();
      if (entry_size == 0) {
        continue;
      }

      read_buffer.resize(entry_size);
      if (ReadEntryValue(entry, read_buffer)) {
        entry.data->UpdateFromProcess(read_buffer);
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

MemoryAddress CheatTableModel::ResolvePointerChain(
    const CheatTableEntry& entry) const {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    return 0;
  }

  // Resolve the base address
  MemoryAddress base_address = entry.pointer_base;

  // If a module name is specified, resolve it to the actual module base
  if (!entry.pointer_module.empty()) {
    auto modules = active_process_->GetModules();
    for (const auto& module : modules) {
      if (module.name == entry.pointer_module) {
        base_address = module.base + entry.pointer_module_offset;
        break;
      }
    }
  }

  // Start with base address
  MemoryAddress current = base_address;

  // Follow the chain of offsets
  for (size_t i = 0; i < entry.pointer_offsets.size(); ++i) {
    // Read pointer at current address
    std::array<std::byte, 8> ptr_buffer;
    const size_t ptr_size = active_process_->GetPointerSize();

    if (!active_process_->ReadMemory(
            {&current, 1}, ptr_size, {ptr_buffer.data(), ptr_size})) {
      return 0;  // Failed to read pointer
    }

    // Extract pointer value.
    uint64_t ptr_value = 0;
    if (ptr_size == 4) {
      std::array<std::byte, 4> temp;
      std::ranges::copy(std::span(ptr_buffer.begin(), 4), temp.begin());
      ptr_value = std::bit_cast<uint32_t>(temp);
    } else if (ptr_size == 8) {
      ptr_value = std::bit_cast<uint64_t>(ptr_buffer);
    }

    if (ptr_value == 0) {
      return 0;  // Null pointer in chain
    }

    // Apply offset (can be negative)
    current = ptr_value + entry.pointer_offsets[i];
  }

  return current;
}

bool CheatTableModel::ReadEntryValue(const CheatTableEntry& entry,
                                     std::span<std::byte> out_buffer) {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    return false;
  }

  MemoryAddress addr;
  if (entry.IsPointerChain()) {
    addr = ResolvePointerChain(entry);
    if (addr == 0) {
      return false;  // Failed to resolve pointer chain
    }
  } else {
    addr = entry.address;
  }

  return active_process_->ReadMemory({&addr, 1}, out_buffer.size(), out_buffer);
}

bool CheatTableModel::WriteEntryValue(const CheatTableEntry& entry,
                                      std::span<const std::byte> data) {
  if (!active_process_ || !active_process_->IsProcessValid()) {
    return false;
  }

  MemoryAddress addr;
  if (entry.IsPointerChain()) {
    addr = ResolvePointerChain(entry);
    if (addr == 0) {
      return false;  // Failed to resolve pointer chain
    }
  } else {
    addr = entry.address;
  }

  return active_process_->WriteMemory(addr, data);
}

}  // namespace maia
