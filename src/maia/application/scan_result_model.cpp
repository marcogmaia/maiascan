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

// Helper to check a single type
// Check that f.operator()<T>() is valid and returns something convertible to
// bool.
template <typename F, typename T>
concept CheckScanType = requires(F& f) {
  { f.template operator()<T>() } -> std::convertible_to<bool>;
};

// The main concept: Ensures the functor works for all critical variants
template <typename F>
concept CScanPredicate = CheckScanType<F, uint8_t> &&
                         CheckScanType<F, uint64_t> && CheckScanType<F, double>;

// We don't need to list every single integer; checking the boundaries
// (u8, u64, double) usually covers the generic lambda's validity.

// clang-format off
// Runtime enum to a compile-time template instantiation.
template <CScanPredicate Func>
constexpr auto DispatchScanType(ScanValueType type, Func&& func) {
  switch (type) {
    case ScanValueType::kUInt8:  return func.template operator()<uint8_t>();
    case ScanValueType::kUInt16: return func.template operator()<uint16_t>();
    case ScanValueType::kUInt32: return func.template operator()<uint32_t>();
    case ScanValueType::kUInt64: return func.template operator()<uint64_t>();
    case ScanValueType::kInt8:   return func.template operator()<int8_t>();
    case ScanValueType::kInt16:  return func.template operator()<int16_t>();
    case ScanValueType::kInt32:  return func.template operator()<int32_t>();
    case ScanValueType::kInt64:  return func.template operator()<int64_t>();
    case ScanValueType::kFloat:  return func.template operator()<float>();
    case ScanValueType::kDouble: return func.template operator()<double>();
    default:
      return false;
  }
}

constexpr size_t GetDataTypeSize(ScanValueType type) {
  switch (type) {
    case ScanValueType::kUInt8:  return sizeof(uint8_t);
    case ScanValueType::kUInt16: return sizeof(uint16_t);
    case ScanValueType::kUInt32: return sizeof(uint32_t);
    case ScanValueType::kUInt64: return sizeof(uint64_t);
    case ScanValueType::kInt8:   return sizeof(int8_t);
    case ScanValueType::kInt16:  return sizeof(int16_t);
    case ScanValueType::kInt32:  return sizeof(int32_t);
    case ScanValueType::kInt64:  return sizeof(int64_t);
    case ScanValueType::kFloat:  return sizeof(float);
    case ScanValueType::kDouble: return sizeof(double);
    default:
      return sizeof(uint32_t);
  }
}

// clang-format on

}  // namespace

void ScanResultModel::FirstScan() {
  std::scoped_lock lock(mutex_);

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for first scan.");
    return;
  }

  prev_entries_ = std::move(curr_entries_);
  curr_entries_.clear();

  if (scan_comparison_ != ScanComparison::kExactValue) {
    scan_comparison_ = ScanComparison::kUnknown;
  }

  const bool is_exact_scan = (scan_comparison_ == ScanComparison::kExactValue);
  size_t scan_stride = 0;

  if (is_exact_scan) {
    if (target_scan_value_.empty()) {
      LogWarning("FirstScan (Exact) requested, but Target Value is empty.");
      return;
    }
    scan_stride = target_scan_value_.size();
  } else {
    scan_stride = GetDataTypeSize(scan_value_type_);
  }

  if (scan_stride == 0) {
    return;
  }

  // Heuristic: Unknown scans create massive lists; reserve carefully.
  // Realistically, 'Unknown' on 1GB of RAM = 250 million entries.
  // This vector implementation will consume ~9GB RAM for 1GB scan.
  // Warning: This needs optimization, our entries are naive.
  curr_entries_.reserve(is_exact_scan ? 4096 : 100000);

  auto regions = active_process_->GetMemoryRegions();

  for (const auto& region : regions) {
    if (!IsReadable(region.protection)) {
      continue;
    }

    // Read the entire region
    std::vector<std::byte> region_buffer(region.size);
    if (!active_process_->ReadMemory(
            {&region.base, 1}, region.size, region_buffer)) {
      continue;
    }

    auto it = region_buffer.begin();
    const auto end = region_buffer.end();

    // --- STRATEGY A: Exact Value (Search) ---
    if (is_exact_scan) {
      const auto val_begin = target_scan_value_.begin();
      const auto val_end = target_scan_value_.end();

      while (true) {
        it = std::search(it, end, val_begin, val_end);
        if (it == end) {
          break;
        }

        uintptr_t found_address =
            region.base + std::distance(region_buffer.begin(), it);
        curr_entries_.emplace_back(found_address, target_scan_value_);

        // Advance by 1 (standard cheat engine behavior to find
        // overlapping/unaligned) Or advance by scan_stride for aligned-only
        // speed.
        std::advance(it, 1);
      }
    }
    // --- STRATEGY B: Unknown Initial Value (Snapshot) ---
    else {
      // Loop through the buffer respecting alignment stride
      // e.g., if stride is 4, we grab 0, 4, 8, 12...
      // We do NOT grab 1, 2, 3 because "Unknown" scans usually assume alignment
      // to reduce the result set size.

      const size_t limit = region_buffer.size() - scan_stride;

      for (size_t offset = 0; offset <= limit; offset += scan_stride) {
        // Create a snapshot of this address
        auto data_start = region_buffer.begin() + offset;

        curr_entries_.emplace_back(
            region.base + offset,
            std::vector<std::byte>(data_start, data_start + scan_stride));
      }
    }
  }

  signals_.memory_changed.publish(curr_entries_);
}

void ScanResultModel::SetActiveProcess(IProcess* process) {
  active_process_ = process;
}

void ScanResultModel::Clear() {
  curr_entries_.clear();
  prev_entries_.clear();
}

void ScanResultModel::StartAutoUpdate() {
  // If already running, do nothing.
  if (task_.joinable()) {
    return;
  }

  task_ = std::jthread([this](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      UpdateCurrentValues();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

  if (curr_entries_.empty()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for next scan.");
    return;
  }

  prev_entries_ = curr_entries_;

  const auto addresses = curr_entries_ |
                         std::views::transform(&ScanEntry::address) |
                         std::ranges::to<std::vector<MemoryAddress>>();

  const size_t value_size = curr_entries_[0].data.size();
  std::vector<std::byte> buffer(addresses.size() * value_size);

  if (!active_process_->ReadMemory(addresses, value_size, buffer)) {
    LogWarning("Failed to read memory during next scan.");
    return;
  }

  const auto check_condition = [&](std::span<const std::byte> curr,
                                   std::span<const std::byte> prev) -> bool {
    switch (scan_comparison_) {
      case ScanComparison::kChanged:
        return IsValueChanged(curr, prev);
      case ScanComparison::kUnchanged:
        return !IsValueChanged(curr, prev);

      case ScanComparison::kIncreased:
        return DispatchScanType(scan_value_type_, [&]<typename T>() {
          return IsValueIncreased<T>(curr, prev);
        });

      case ScanComparison::kDecreased:
        return DispatchScanType(scan_value_type_, [&]<typename T>() {
          return IsValueDecreased<T>(curr, prev);
        });

      default:
        return false;
    }
  };

  std::vector<ScanEntry> filtered_entries;
  filtered_entries.reserve(curr_entries_.size());

  const std::byte* cursor = buffer.data();
  for (size_t i = 0; i < curr_entries_.size(); ++i) {
    std::span<const std::byte> current_bytes(cursor, value_size);
    std::span<const std::byte> prev_bytes(prev_entries_[i].data);

    if (check_condition(current_bytes, prev_bytes)) {
      filtered_entries.emplace_back(
          prev_entries_[i].address,
          std::vector<std::byte>(current_bytes.begin(), current_bytes.end()));
    }

    cursor += value_size;
  }

  curr_entries_ = std::move(filtered_entries);
  signals_.memory_changed.publish(curr_entries_);
}

void ScanResultModel::UpdateCurrentValues() {
  std::scoped_lock lock(mutex_);

  if (curr_entries_.empty() || !CanScan(active_process_)) {
    return;
  }

  auto addresses = curr_entries_ | std::views::transform(&ScanEntry::address) |
                   std::ranges::to<std::vector<MemoryAddress>>();

  const size_t value_size = curr_entries_[0].data.size();
  std::vector<std::byte> buffer(addresses.size() * value_size);

  if (!active_process_->ReadMemory(addresses, value_size, buffer)) {
    return;
  }

  auto new_values = buffer | std::views::chunk(value_size);

  for (auto [entry, new_val] : std::views::zip(curr_entries_, new_values)) {
    std::ranges::copy(new_val, entry.data.begin());
  }

  signals_.memory_changed.publish(curr_entries_);
}

ScanResultModel::ScanResultModel() {
  StartAutoUpdate();
};

}  // namespace maia
