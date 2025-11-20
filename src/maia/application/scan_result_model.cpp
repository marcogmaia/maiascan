// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include "maia/logging.h"
#include "maia/mmem/mmem.h"

namespace maia {

namespace {

constexpr bool IsReadable(mmem::Protection prot) noexcept {
  const auto prot_val = static_cast<uint32_t>(prot);
  const auto read_val = static_cast<uint32_t>(mmem::Protection::kRead);
  return (prot_val & read_val) != 0;
}

bool CanScan(IProcess* process) {
  return process && process->IsProcessValid();
}

// Safely loads a value from a byte span.
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr T LoadValue(std::span<const std::byte> bytes) {
  std::array<std::byte, sizeof(T)> buffer{};
  const size_t copy_size = std::min(bytes.size(), sizeof(T));
  std::copy_n(bytes.begin(), copy_size, buffer.begin());
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

template <typename F, typename T>
concept CheckScanType = requires(F& f) {
  { f.template operator()<T>() } -> std::convertible_to<bool>;
};

template <typename F>
concept CScanPredicate = CheckScanType<F, uint8_t> &&
                         CheckScanType<F, uint64_t> && CheckScanType<F, double>;

// clang-format off
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
    default: return false;
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
    default: return sizeof(uint32_t);
  }
}

// clang-format on

void ClearStorage(ScanStorage& storage) {
  storage.addresses.clear();
  storage.raw_values_buffer.clear();
  storage.stride = 0;
}

}  // namespace

ScanResultModel::ScanResultModel() {
  StartAutoUpdate();
}

void ScanResultModel::FirstScan() {
  std::scoped_lock lock(mutex_);
  LogInfo("First scan...");

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for first scan.");
    return;
  }

  // Archive current results to previous (optional, but good for undo logic
  // later)
  prev_entries_ = std::move(curr_entries_);
  ClearStorage(curr_entries_);

  // Initialize comparison logic
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

  // Temporary local storage for building results
  ScanStorage storage;
  storage.stride = scan_stride;

  // Reservation heuristics (adjust based on available RAM/preferences)
  constexpr size_t kEstimatedEntries = 100000;
  storage.addresses.reserve(kEstimatedEntries);
  storage.raw_values_buffer.reserve(kEstimatedEntries * scan_stride);

  auto regions = active_process_->GetMemoryRegions();

  for (const auto& region : regions) {
    if (!IsReadable(region.protection)) {
      continue;
    }

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

        storage.addresses.push_back(found_address);
        storage.raw_values_buffer.insert(
            storage.raw_values_buffer.end(), val_begin, val_end);

        std::advance(it, 1);
      }
    }
    // --- STRATEGY B: Unknown Initial Value (Snapshot) ---
    else {
      const size_t limit = region_buffer.size() - scan_stride;

      for (size_t offset = 0; offset <= limit; offset += scan_stride) {
        auto data_start = region_buffer.begin() + offset;

        storage.addresses.push_back(region.base + offset);
        storage.raw_values_buffer.insert(storage.raw_values_buffer.end(),
                                         data_start,
                                         data_start + scan_stride);
      }
    }
  }

  // Commit results.
  curr_entries_ = std::move(storage);
  signals_.memory_changed.publish(curr_entries_);

  LogInfo("Found {} addresses.", curr_entries_.addresses.size());
}

void ScanResultModel::NextScan() {
  std::scoped_lock lock(mutex_);

  // Validate State
  if (curr_entries_.addresses.empty()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for next scan.");
    return;
  }

  // Move Current -> Previous (Snapshot)
  // This makes curr_entries_ empty and ready to accept filtered results.
  prev_entries_ = std::move(curr_entries_);

  const size_t count = prev_entries_.addresses.size();
  const size_t stride = prev_entries_.stride;

  if (count == 0 || stride == 0) {
    return;
  }

  // Read Current Memory (Batch)
  // We read the live values of all addresses found in the previous scan.
  std::vector<std::byte> current_memory_buffer(count * stride);
  if (!active_process_->ReadMemory(
          prev_entries_.addresses, stride, current_memory_buffer)) {
    LogWarning("Failed to read memory batch during next scan.");
    // Restore previous entries on failure so we don't lose data
    curr_entries_ = std::move(prev_entries_);
    return;
  }

  // Prepare Filtered Storage
  ScanStorage filtered_storage;
  filtered_storage.stride = stride;
  filtered_storage.addresses.reserve(count);
  filtered_storage.raw_values_buffer.reserve(count * stride);

  // Comparison Logic
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

      case ScanComparison::kExactValue:
        return std::ranges::equal(curr, target_scan_value_);

      default:
        return false;
    }
  };

  // Filter Loop (Structure of Arrays)
  const std::byte* curr_ptr = current_memory_buffer.data();
  const std::byte* prev_ptr = prev_entries_.raw_values_buffer.data();

  for (size_t i = 0; i < count; ++i) {
    std::span<const std::byte> val_curr(curr_ptr, stride);
    std::span<const std::byte> val_prev(prev_ptr, stride);

    if (check_condition(val_curr, val_prev)) {
      // Match Found: Keep Address + NEW Value
      filtered_storage.addresses.push_back(prev_entries_.addresses[i]);
      filtered_storage.raw_values_buffer.insert(
          filtered_storage.raw_values_buffer.end(),
          val_curr.begin(),
          val_curr.end());
    }

    curr_ptr += stride;
    prev_ptr += stride;
  }

  // Commit Results
  curr_entries_ = std::move(filtered_storage);
  signals_.memory_changed.publish(curr_entries_);
}

void ScanResultModel::UpdateCurrentValues() {
  std::scoped_lock lock(mutex_);

  if (curr_entries_.addresses.empty() || !CanScan(active_process_)) {
    return;
  }

  // Bulk read directly into the existing raw_values_buffer.
  // Since curr_entries_ uses SoA, raw_values_buffer is already a flat vector
  // exactly the size needed (addresses.size() * stride).
  bool success = active_process_->ReadMemory(curr_entries_.addresses,
                                             curr_entries_.stride,
                                             curr_entries_.raw_values_buffer);

  if (success) {
    signals_.memory_changed.publish(curr_entries_);
  }
}

void ScanResultModel::SetActiveProcess(IProcess* process) {
  std::scoped_lock lock(mutex_);
  if (!CanScan(process)) {
    LogWarning("Invalid process selected.");
    return;
  }
  active_process_ = process;
  LogInfo("Active process changed: {}", process->GetProcessName());
}

void ScanResultModel::Clear() {
  std::scoped_lock lock(mutex_);
  ClearStorage(curr_entries_);
  ClearStorage(prev_entries_);
}

void ScanResultModel::StartAutoUpdate() {
  if (task_.joinable()) {
    return;
  }

  task_ = std::jthread([this](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      if (curr_entries_.addresses.size() < 10000) {
        UpdateCurrentValues();
      }
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

}  // namespace maia
