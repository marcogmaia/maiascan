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

// We check flags to stop crashes when reading protected memory like guard
// pages.
constexpr bool IsReadable(mmem::Protection prot) noexcept {
  const auto prot_val = static_cast<uint32_t>(prot);
  const auto read_val = static_cast<uint32_t>(mmem::Protection::kRead);
  return (prot_val & read_val) != 0;
}

bool CanScan(IProcess* process) {
  return process && process->IsProcessValid();
}

// Copying to a local array fixes crashes on systems that hate unaligned memory
// reads before casting.
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr T LoadValue(std::span<const std::byte> bytes) {
  std::array<std::byte, sizeof(T)> buffer{};
  const size_t copy_size = std::min(bytes.size(), sizeof(T));
  std::copy_n(bytes.begin(), copy_size, buffer.begin());
  return std::bit_cast<T>(buffer);
}

// Byte comparison is faster than decoding types and catches all changes
// including padding or NaN data.
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

// This helper stops us from writing the same switch statement inside every loop
// by connecting runtime enums to compile-time templates.
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
  storage.curr_raw.clear();
  storage.prev_raw.clear();
  storage.stride = 0;
}

}  // namespace

void ScanResultModel::FirstScan() {
  // Holding the lock stops background threads from breaking the vectors while
  // we rebuild them.
  std::scoped_lock lock(mutex_);
  LogInfo("First scan...");

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for first scan.");
    return;
  }

  // Saving the old state lets us compare against the start of the scan later
  // if needed.
  scan_storage_.prev_raw = std::move(scan_storage_.curr_raw);
  ClearStorage(scan_storage_);

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

  // We build results locally to avoid locking issues. The split address/data
  // layout helps the CPU cache work better during updates.
  //
  ScanStorage storage;
  storage.stride = scan_stride;

  // Scans can create millions of results so we reserve memory now to stop
  // slow reallocations later.
  constexpr size_t kEstimatedEntries = 100000;
  storage.addresses.reserve(kEstimatedEntries);
  storage.curr_raw.reserve(kEstimatedEntries * scan_stride);

  auto regions = active_process_->GetMemoryRegions();

  for (const auto& region : regions) {
    if (!IsReadable(region.protection)) {
      continue;
    }

    // Reading the whole block at once is much faster than making thousands
    // of slow system calls.
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
        // Standard library search uses vectorization which is faster than
        // manual loops.
        it = std::search(it, end, val_begin, val_end);
        if (it == end) {
          break;
        }

        uintptr_t found_address =
            region.base + std::distance(region_buffer.begin(), it);

        storage.addresses.push_back(found_address);
        storage.curr_raw.insert(storage.curr_raw.end(), val_begin, val_end);

        // We move by one byte to find values that might not sit on standard
        // boundaries.
        std::advance(it, 1);
      }
    }
    // --- STRATEGY B: Unknown Initial Value (Snapshot) ---
    else {
      const size_t limit = region_buffer.size() - scan_stride;

      // We stick to the data size steps here. Saving every single byte would
      // fill up RAM instantly.
      for (size_t offset = 0; offset <= limit; offset += scan_stride) {
        auto data_start = region_buffer.begin() + offset;

        storage.addresses.push_back(region.base + offset);
        storage.curr_raw.insert(
            storage.curr_raw.end(), data_start, data_start + scan_stride);
      }
    }
  }

  scan_storage_ = std::move(storage);
  signals_.memory_changed.publish(scan_storage_);

  LogInfo("Found {} addresses.", scan_storage_.addresses.size());
}

void ScanResultModel::NextScan() {
  std::scoped_lock lock(mutex_);

  if (scan_storage_.addresses.empty()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for next scan.");
    return;
  }

  // Moving current values to previous gives us a baseline to see what changed.
  scan_storage_.prev_raw = std::move(scan_storage_.curr_raw);

  const size_t count = scan_storage_.addresses.size();
  const size_t stride = scan_storage_.stride;

  if (count == 0 || stride == 0) {
    return;
  }

  // Reading all addresses in one go stops the app from freezing due to too
  // many system calls.
  std::vector<std::byte> current_memory_buffer(count * stride);
  if (!active_process_->ReadMemory(
          scan_storage_.addresses, stride, current_memory_buffer)) {
    LogWarning("Failed to read memory batch during next scan.");
    // Put data back if reading fails so we do not lose our state.
    scan_storage_.curr_raw = std::move(scan_storage_.prev_raw);
    return;
  }

  // Building a new vector is faster than removing items from the middle of
  // an existing one.
  ScanStorage filtered_storage;
  filtered_storage.stride = stride;
  filtered_storage.addresses.reserve(count / 2);
  filtered_storage.curr_raw.reserve((count / 2) * stride);
  filtered_storage.prev_raw.reserve((count / 2) * stride);

  // Define logic here to keep the loop clean.
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

  // Using pointers is faster here than iterators for simple math.
  const std::byte* curr_ptr = current_memory_buffer.data();
  const std::byte* prev_ptr = scan_storage_.prev_raw.data();

  if (scan_storage_.prev_raw.size() != count * stride) {
    LogWarning("Mismatch in previous raw data size. Aborting NextScan.");
    scan_storage_.curr_raw = std::move(scan_storage_.prev_raw);
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    std::span<const std::byte> val_curr(curr_ptr, stride);
    std::span<const std::byte> val_prev(prev_ptr, stride);

    if (check_condition(val_curr, val_prev)) {
      filtered_storage.addresses.push_back(scan_storage_.addresses[i]);
      // Keep both values so the UI can show what changed.
      filtered_storage.curr_raw.insert(
          filtered_storage.curr_raw.end(), val_curr.begin(), val_curr.end());
      filtered_storage.prev_raw.insert(
          filtered_storage.prev_raw.end(), val_prev.begin(), val_prev.end());
    }

    curr_ptr += stride;
    prev_ptr += stride;
  }

  scan_storage_ = std::move(filtered_storage);
  signals_.memory_changed.publish(scan_storage_);

  LogInfo("Next scan complete. {} addresses remaining.",
          scan_storage_.addresses.size());
}

void ScanResultModel::UpdateCurrentValues() {
  std::scoped_lock lock(mutex_);

  if (scan_storage_.addresses.empty() || !CanScan(active_process_)) {
    return;
  }

  // We resize now to avoid issues if the list size changed but the buffer
  // did not catch up yet.
  size_t required_size = scan_storage_.addresses.size() * scan_storage_.stride;
  if (scan_storage_.curr_raw.size() != required_size) {
    scan_storage_.curr_raw.resize(required_size);
  }

  bool success = active_process_->ReadMemory(
      scan_storage_.addresses, scan_storage_.stride, scan_storage_.curr_raw);

  if (success) {
    signals_.memory_changed.publish(scan_storage_);
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
  ClearStorage(scan_storage_);
  signals_.memory_changed.publish(scan_storage_);
}

void ScanResultModel::StartAutoUpdate() {
  if (task_.joinable()) {
    return;
  }

  task_ = std::jthread([this](std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      // We limit updates to 10k items because refreshing millions freezes the
      // UI.
      size_t count = 0;
      {
        std::scoped_lock lock(mutex_);
        count = scan_storage_.addresses.size();
      }

      if (count > 0 && count < 10000) {
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
