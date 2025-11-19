// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <span>

#include "maia/logging.h"
#include "maia/mmem/mmem.h"

namespace maia {

namespace {

constexpr bool IsReadable(mmem::Protection prot) noexcept {
  const auto prot_val = static_cast<uint32_t>(prot);
  const auto read_val = static_cast<uint32_t>(mmem::Protection::kRead);
  return (prot_val & read_val) != 0;
}

std::optional<std::vector<std::byte>> ReadRegion(const MemoryRegion& region,
                                                 IProcess& process) {
  std::vector<std::byte> buffer(region.size);
  if (!process.ReadMemory({&region.base, 1}, region.size, buffer)) {
    return std::nullopt;
  }
  return buffer;
}

bool CanScan(IProcess* process) {
  return process && process->IsProcessValid();
}

// Safely loads a value from a byte span.
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr T LoadValue(std::span<const std::byte> bytes) {
  std::array<std::byte, sizeof(T)> buffer{};
  std::copy_n(bytes.begin(), sizeof(T), buffer.begin());
  return std::bit_cast<T>(buffer);
}

constexpr bool IsValueChanged(std::span<const std::byte> current,
                              std::span<const std::byte> previous) {
  if (current.size() != previous.size()) {
    return false;
  }
  return !std::ranges::equal(current, previous);
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr bool IsValueDecreased(std::span<const std::byte> current,
                                std::span<const std::byte> previous) {
  if (current.size() < sizeof(T) || previous.size() < sizeof(T)) {
    return false;
  }
  return LoadValue<T>(current) < LoadValue<T>(previous);
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr bool IsValueIncreased(std::span<const std::byte> current,
                                std::span<const std::byte> previous) {
  if (current.size() < sizeof(T) || previous.size() < sizeof(T)) {
    return false;
  }

  return LoadValue<T>(current) > LoadValue<T>(previous);
}

}  // namespace

void ScanResultModel::FirstScan(std::vector<std::byte> value_to_scan) {
  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid.");
    return;
  }
  // Lock the mutex for the entire operation
  std::scoped_lock lock(mutex_);

  prev_entries_ = std::move(entries_);
  entries_.clear();

  if (value_to_scan.empty()) {
    signals_.memory_changed.publish(entries_);
    return;
  }

  constexpr auto kPageSize = 4096;
  std::vector<ScanEntry> new_entries;
  new_entries.reserve(kPageSize);

  auto regions = active_process_->GetMemoryRegions();

  for (const auto& region : regions) {
    // Skip non-readable regions
    if (!IsReadable(region.protection)) {
      continue;
    }

    // Read the entire region.
    std::optional<std::vector<std::byte>> region_buffer =
        ReadRegion(region, *active_process_);
    if (!region_buffer) {
      continue;
    }

    // Search for the value pattern in this region.
    auto& read_buffer = *region_buffer;
    auto it = read_buffer.begin();
    auto end = read_buffer.end();
    while (true) {
      it = std::search(it, end, value_to_scan.begin(), value_to_scan.end());
      if (it == end) {
        break;  // Not found
      }

      size_t offset = std::distance(read_buffer.begin(), it);
      new_entries.emplace_back(region.base + offset, value_to_scan);

      // Move past this match to find the next one.
      std::advance(it, value_to_scan.size());
    }
  }

  entries_ = std::move(new_entries);
  signals_.memory_changed.publish(entries_);

  StartAutoUpdate();
}

void ScanResultModel::SetActiveProcess(IProcess* process) {
  active_process_ = process;
}

void ScanResultModel::Clear() {
  entries_.clear();
  prev_entries_.clear();
}

void ScanResultModel::UpdateCurrentValues() {
  std::scoped_lock lock(mutex_);

  if (entries_.empty() || !CanScan(active_process_)) {
    return;
  }

  // We assume all entries have the same size based on the first scan logic
  const size_t value_size = entries_[0].data.size();

  auto addresses = entries_ | std::views::transform(&ScanEntry::address) |
                   std::ranges::to<std::vector<MemoryAddress>>();

  std::vector<std::byte> buffer(addresses.size() * value_size);

  if (!active_process_->ReadMemory(addresses, value_size, buffer)) {
    return;
  }

  auto new_values = buffer | std::views::chunk(value_size);

  for (auto [entry, new_val] : std::views::zip(entries_, new_values)) {
    std::ranges::copy(new_val, entry.data.begin());
  }

  signals_.memory_changed.publish(entries_);
}

void ScanResultModel::StartAutoUpdate() {
  // If already running, do nothing.
  if (task_.joinable()) {
    return;
  }

  task_ = std::jthread([this](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      UpdateCurrentValues();
    }
  });
}

void ScanResultModel::StopAutoUpdate() {
  if (task_.joinable()) {
    task_.request_stop();
    task_.join();
  }
}

void ScanResultModel::NextScan() {
  std::scoped_lock lock(mutex_);

  if (entries_.empty()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for next scan.");
    return;
  }

  // Validate uniform value size across entries
  const size_t value_size = entries_[0].data.size();
  if (value_size == 0 || value_size > 8) {
    LogWarning("Invalid value size {} for next scan.", value_size);
    return;
  }

  // Store previous scan results for comparison
  prev_entries_ = entries_;  // Copy, not move

  // Extract all addresses for efficient batch reading
  const std::vector<MemoryAddress> addresses =
      entries_ | std::views::transform(&ScanEntry::address) |
      std::ranges::to<std::vector<MemoryAddress>>();

  // #ifndef NDEBUG
  //   // Debug check for uniform size
  //   if (!std::ranges::all_of(entries_, [value_size](const auto& e) {
  //         return e.data.size() == value_size;
  //       })) {
  //     LogWarning("Value sizes are not uniform across entries.");
  //     return;
  //   }
  // #endif

  // Read current values from all addresses in a single batch operation
  std::vector<std::byte> buffer(addresses.size() * value_size);
  if (!active_process_->ReadMemory(addresses, value_size, buffer)) {
    LogWarning("Failed to read memory during next scan.");
    return;
  }

  // Filter entries based on comparison mode
  std::vector<ScanEntry> filtered_entries;
  filtered_entries.reserve(entries_.size());

  for (size_t i = 0; i < entries_.size(); ++i) {
    const auto& prev_entry = prev_entries_[i];
    std::span<const std::byte> current_bytes(&buffer[i * value_size],
                                             value_size);
    std::span<const std::byte> prev_bytes(prev_entry.data);

    bool should_keep = false;

    switch (scan_comparison_) {
        // clang-format off
      case ScanComparison::kChanged:   should_keep = !std::ranges::equal(current_bytes, prev_bytes); break;
      case ScanComparison::kUnchanged: should_keep =  std::ranges::equal(current_bytes, prev_bytes); break;

      case ScanComparison::kIncreased:
        switch (value_size) {
          case 1: should_keep = IsValueIncreased<uint8_t>(current_bytes, prev_bytes); break;
          case 2: should_keep = IsValueIncreased<uint16_t>(current_bytes, prev_bytes); break;
          case 4: should_keep = IsValueIncreased<uint32_t>(current_bytes, prev_bytes); break;
          case 8: should_keep = IsValueIncreased<uint64_t>(current_bytes, prev_bytes); break;
        }
        break;

      case ScanComparison::kDecreased:
        switch (value_size) {
          case 1: should_keep = IsValueDecreased<uint8_t>(current_bytes, prev_bytes); break;
          case 2: should_keep = IsValueDecreased<uint16_t>(current_bytes, prev_bytes); break;
          case 4: should_keep = IsValueDecreased<uint32_t>(current_bytes, prev_bytes); break;
          case 8: should_keep = IsValueDecreased<uint64_t>(current_bytes, prev_bytes); break;
        }
        break;
        // clang-format on

      case ScanComparison::kIncreasedBy:
      case ScanComparison::kDecreasedBy:
        // TODO: Add int64_t compare_value_ member with setter
        // should_keep = (current_val - prev_val) == compare_value_;
        LogWarning(
            "Scan comparison 'IncreasedBy/DecreasedBy' requires delta value "
            "parameter");
        break;

      case ScanComparison::kExactValue:
      case ScanComparison::kNotEqual:
      case ScanComparison::kGreaterThan:
      case ScanComparison::kLessThan:
      case ScanComparison::kBetween:
      case ScanComparison::kNotBetween:
        // TODO: Add std::vector<std::byte> target_value_ member(s) with setters
        LogWarning("Scan comparison mode requires target value parameter(s)");
        break;

      case ScanComparison::kUnknown:
      default:
        LogWarning("Unknown scan comparison mode: {}",
                   static_cast<int>(scan_comparison_));
        break;
    }

    if (should_keep) {
      // Update entry with current value (preserve address, update data)
      filtered_entries.emplace_back(
          prev_entry.address,
          std::vector<std::byte>(current_bytes.begin(), current_bytes.end()));
    }
  }

  // Replace entries with filtered results
  entries_ = std::move(filtered_entries);

  // Notify UI/components of updated results
  signals_.memory_changed.publish(entries_);
}

}  // namespace maia
