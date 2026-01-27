// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <algorithm>
#include <array>
#include <future>
#include <span>
#include <thread>
#include <vector>

#include "maia/assert.h"
#include "maia/core/scoped_process_suspend.h"
#include "maia/core/simd_scanner.h"
#include "maia/logging.h"
#include "maia/mmem/mmem.h"

namespace maia {

namespace {

// Represents a single chunk of memory to scan.
struct ScanTask {
  uintptr_t base_address;  // Virtual address in the target process
  size_t scan_size;        // Logical chunk size (what we report matches within)
  size_t read_size;  // Actual bytes to read (includes overlap for patterns)
};

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

template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr bool IsValueIncreasedBy(std::span<const std::byte> current,
                                  std::span<const std::byte> previous,
                                  std::span<const std::byte> target) {
  if (current.size() < sizeof(T) || previous.size() < sizeof(T) ||
      target.size() < sizeof(T)) {
    return false;
  }
  return LoadValue<T>(current) == LoadValue<T>(previous) + LoadValue<T>(target);
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr bool IsValueDecreasedBy(std::span<const std::byte> current,
                                  std::span<const std::byte> previous,
                                  std::span<const std::byte> target) {
  if (current.size() < sizeof(T) || previous.size() < sizeof(T) ||
      target.size() < sizeof(T)) {
    return false;
  }
  return LoadValue<T>(current) == LoadValue<T>(previous) - LoadValue<T>(target);
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
    case ScanValueType::kString:
    case ScanValueType::kWString:
    case ScanValueType::kArrayOfBytes:
      return false; // Not supported for arithmetic comparisons
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
    case ScanValueType::kString:
    case ScanValueType::kWString:
    case ScanValueType::kArrayOfBytes:
      return 1; // Byte alignment by default for variable types
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

template <typename CheckFunc>
void PerformNextScanScalar(const std::vector<std::byte>& current_memory_buffer,
                           const ScanStorage& scan_storage,
                           size_t count,
                           size_t stride,
                           ScanStorage& filtered_storage,
                           CheckFunc&& check_condition) {
  const std::byte* curr_ptr = current_memory_buffer.data();
  const std::byte* prev_ptr = scan_storage.prev_raw.data();

  for (size_t i = 0; i < count; ++i) {
    std::span<const std::byte> val_curr(curr_ptr, stride);
    std::span<const std::byte> val_prev(prev_ptr, stride);

    if (check_condition(val_curr, val_prev, i)) {
      filtered_storage.addresses.emplace_back(scan_storage.addresses[i]);

      filtered_storage.curr_raw.insert(
          filtered_storage.curr_raw.end(), val_curr.begin(), val_curr.end());

      filtered_storage.prev_raw.insert(
          filtered_storage.prev_raw.end(), val_curr.begin(), val_curr.end());
    }

    curr_ptr += stride;
    prev_ptr += stride;
  }
}

}  // namespace

ScanResultModel::ScanResultModel(std::unique_ptr<core::ITaskRunner> task_runner,
                                 size_t chunk_size)
    : task_runner_(std::move(task_runner)),
      chunk_size_(chunk_size) {
  if (!task_runner_) {
    task_runner_ = std::make_unique<core::AsyncTaskRunner>();
  }
}

void ScanResultModel::FirstScan() {
  if (is_scanning_.load()) {
    return;
  }

  // Clear existing results immediately so UI shows empty state.
  {
    std::scoped_lock lock(mutex_);
    ClearStorage(scan_storage_);
    signals_.memory_changed.publish(scan_storage_);
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for first scan.");
    return;
  }

  LogInfo("Starting first scan...");
  is_scanning_ = true;
  progress_ = 0.0f;
  scan_finished_flag_ = false;

  task_runner_->Run(
      [this](std::stop_token stop_token) { ExecuteScanWorker(stop_token); });
}

void ScanResultModel::ExecuteScanWorker(std::stop_token stop_token) {
  // Capture necessary state by value to avoid race conditions.
  // Note: For SyncTaskRunner, this copy is less critical but still good
  // practice.
  std::vector<std::byte> target_value;
  std::vector<std::byte> target_mask;
  ScanValueType value_type;
  ScanComparison comparison;
  bool pause_enabled;
  bool fast_scan;
  IProcess* process;

  {
    std::scoped_lock lock(mutex_);
    target_value = target_scan_value_;
    target_mask = target_scan_mask_;
    value_type = scan_value_type_;
    comparison = scan_comparison_;
    pause_enabled = pause_while_scanning_enabled_;
    fast_scan = fast_scan_enabled_;
    process = active_process_;
  }

  std::optional<ScopedProcessSuspend> suspend;
  if (pause_enabled) {
    suspend.emplace(process);
  }

  ScanComparison effective_comparison = comparison;
  if (effective_comparison != ScanComparison::kExactValue) {
    effective_comparison = ScanComparison::kUnknown;
  }

  const bool is_exact_scan =
      (effective_comparison == ScanComparison::kExactValue);
  size_t scan_stride = 0;

  if (is_exact_scan) {
    if (target_value.empty()) {
      is_scanning_ = false;
      return;
    }
    scan_stride = target_value.size();
  } else {
    scan_stride = GetDataTypeSize(value_type);
  }

  if (scan_stride == 0) {
    is_scanning_ = false;
    return;
  }

  LogInfo("FirstScan start: stride={}, is_exact={}, mask={}",
          scan_stride,
          is_exact_scan,
          !target_mask.empty());

  if (is_exact_scan) {
    std::string pattern_hex;
    for (auto b : target_value) {
      pattern_hex += std::format("{:02X} ", static_cast<uint8_t>(b));
    }
    LogInfo("Pattern: {}", pattern_hex);
  }

  // Determine alignment based on user preference.

  const size_t alignment = fast_scan ? GetDataTypeSize(value_type) : 1;

  ScanStorage storage;
  storage.stride = scan_stride;
  storage.value_type = value_type;

  auto regions = process->GetMemoryRegions();
  size_t readable_count = 0;
  size_t total_size = 0;
  for (const auto& r : regions) {
    if (IsReadable(r.protection)) {
      readable_count++;
      total_size += r.size;
    }
  }
  LogInfo("Scanning {} readable regions (total {} MB)",
          readable_count,
          total_size / (1024 * 1024));

  const size_t overlap_size = scan_stride > 1 ? scan_stride - 1 : 0;
  const size_t overlap = is_exact_scan ? overlap_size : 0;

  std::vector<ScanTask> tasks;
  for (const auto& region : regions) {
    if (stop_token.stop_requested()) {
      break;
    }
    if (!IsReadable(region.protection)) {
      continue;
    }

    const uintptr_t region_end = region.base + region.size;
    uintptr_t current_addr = region.base;

    while (current_addr < region_end) {
      size_t chunk_scan_size = std::min(chunk_size_, region_end - current_addr);
      size_t chunk_read_size = chunk_scan_size + overlap;
      if (current_addr + chunk_read_size > region_end) {
        chunk_read_size = region_end - current_addr;
      }

      tasks.emplace_back(ScanTask{.base_address = current_addr,
                                  .scan_size = chunk_scan_size,
                                  .read_size = chunk_read_size});
      current_addr += chunk_scan_size;
    }
  }

  const size_t total_tasks = tasks.size();
  if (total_tasks == 0) {
    is_scanning_ = false;
    return;
  }

  std::atomic<size_t> processed_tasks{0};

  const size_t num_threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::vector<ScanTask>> thread_batches(num_threads);
  for (size_t i = 0; i < total_tasks; ++i) {
    thread_batches[i % num_threads].emplace_back(tasks[i]);
  }

  auto worker = [process,
                 is_exact_scan,
                 scan_stride,
                 alignment,
                 &target_value,
                 &target_mask,
                 &stop_token,
                 &processed_tasks,
                 total_tasks,
                 this](const std::vector<ScanTask>& batch) -> ScanStorage {
    ScanStorage local_storage;
    local_storage.stride = scan_stride;
    local_storage.addresses.reserve(1024);
    local_storage.curr_raw.reserve(1024 * scan_stride);

    std::vector<std::byte> buffer;

    for (const auto& task : batch) {
      if (stop_token.stop_requested()) {
        return {};
      }

      buffer.resize(task.read_size);
      MemoryAddress addr = task.base_address;
      if (!process->ReadMemory({&addr, 1}, task.read_size, buffer)) {
        processed_tasks++;
        progress_ = static_cast<float>(processed_tasks) /
                    static_cast<float>(total_tasks);
        continue;
      }

      if (is_exact_scan) {
        auto callback = [&](size_t offset) {
          if (offset >= task.scan_size) {
            return;
          }
          LogDebug("Match found at address 0x{:X}", task.base_address + offset);
          local_storage.addresses.emplace_back(task.base_address + offset);
          local_storage.curr_raw.insert(local_storage.curr_raw.end(),
                                        buffer.begin() + offset,
                                        buffer.begin() + offset + scan_stride);
        };

        if (target_mask.empty()) {
          core::ScanBuffer(buffer, target_value, alignment, callback);
        } else {
          core::ScanBufferMasked(buffer, target_value, target_mask, callback);
        }
      } else {
        if (buffer.size() >= scan_stride) {
          const size_t limit =
              std::min(buffer.size() - scan_stride, task.scan_size);
          for (size_t offset = 0; offset <= limit; offset += alignment) {
            auto data_start = buffer.begin() + offset;
            local_storage.addresses.emplace_back(task.base_address + offset);
            local_storage.curr_raw.insert(local_storage.curr_raw.end(),
                                          data_start,
                                          data_start + scan_stride);
          }
        }
      }

      processed_tasks++;
      progress_ =
          static_cast<float>(processed_tasks) / static_cast<float>(total_tasks);
    }
    return local_storage;
  };

  std::vector<std::future<ScanStorage>> futures;
  for (const auto& batch : thread_batches) {
    if (!batch.empty()) {
      futures.emplace_back(std::async(std::launch::async, worker, batch));
    }
  }

  // Merge results
  for (auto& future : futures) {
    if (stop_token.stop_requested()) {
      break;
    }
    ScanStorage result = future.get();
    storage.addresses.insert(storage.addresses.end(),
                             result.addresses.begin(),
                             result.addresses.end());
    storage.curr_raw.insert(
        storage.curr_raw.end(), result.curr_raw.begin(), result.curr_raw.end());
  }

  if (stop_token.stop_requested()) {
    is_scanning_ = false;
    LogInfo("Scan cancelled.");
    return;
  }

  // Store result for main thread
  {
    std::scoped_lock lock(pending_mutex_);
    pending_storage_ = std::move(storage);
    scan_finished_flag_ = true;
  }
}

void ScanResultModel::CancelScan() {
  if (is_scanning_.load()) {
    task_runner_->RequestStop();
  }
}

void ScanResultModel::ApplyPendingResult() {
  if (!scan_finished_flag_.load()) {
    return;
  }

  std::scoped_lock lock(mutex_, pending_mutex_);

  // Initialize previous values with current to create baseline
  pending_storage_.prev_raw = pending_storage_.curr_raw;

  scan_storage_ = std::move(pending_storage_);
  scan_finished_flag_ = false;
  is_scanning_ = false;

  // Clear pending storage to free memory
  ClearStorage(pending_storage_);

  signals_.memory_changed.publish(scan_storage_);
  LogInfo("Scan complete. Found {} addresses.", scan_storage_.addresses.size());
}

void ScanResultModel::NextScan() {
  if (is_scanning_.load()) {
    return;
  }

  std::scoped_lock lock(mutex_);

  std::optional<ScopedProcessSuspend> suspend;
  if (pause_while_scanning_enabled_) {
    suspend.emplace(active_process_);
  }

  if (scan_storage_.addresses.empty()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for next scan.");
    return;
  }

  // We deliberately do NOT move curr_raw to prev_raw here.
  // prev_raw contains the clean snapshot from the LAST scan, while curr_raw
  // might have been modified by the auto-update thread. We compare
  // Live (Freshly Read) vs Snapshot (Clean) to avoid logic errors.

  const size_t count = scan_storage_.addresses.size();
  const size_t stride = scan_storage_.stride;

  if (count == 0 || stride == 0) {
    return;
  }

  // Reading all addresses in one go stops the app from freezing due to too
  // many system calls.
  std::vector<std::byte> current_memory_buffer(count * stride);
  std::vector<uint8_t> success_mask(count, 0);

  if (!active_process_->ReadMemory(scan_storage_.addresses,
                                   stride,
                                   current_memory_buffer,
                                   &success_mask)) {
    LogWarning("Failed to read memory batch during next scan.");
    return;
  }

  // Building a new vector is faster than removing items from the middle of
  // an existing one.

  ScanStorage filtered_storage;
  filtered_storage.stride = stride;
  filtered_storage.value_type = scan_storage_.value_type;
  filtered_storage.addresses.reserve(count / 2);

  filtered_storage.curr_raw.reserve((count / 2) * stride);
  filtered_storage.prev_raw.reserve((count / 2) * stride);

  // Define logic here to keep the loop clean.
  const auto check_condition = [&](std::span<const std::byte> curr,
                                   std::span<const std::byte> prev,
                                   size_t index) -> bool {
    // If we failed to read this address, we simply discard it.
    if (index < success_mask.size() && success_mask[index] == 0) {
      return false;
    }

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

      case ScanComparison::kIncreasedBy:
        return DispatchScanType(scan_value_type_, [&]<typename T>() {
          return IsValueIncreasedBy<T>(
              curr, prev, std::span{target_scan_value_});
        });

      case ScanComparison::kDecreasedBy:
        return DispatchScanType(scan_value_type_, [&]<typename T>() {
          return IsValueDecreasedBy<T>(
              curr, prev, std::span{target_scan_value_});
        });

      case ScanComparison::kExactValue:
        return std::ranges::equal(curr, target_scan_value_);

      default:
        return false;
    }
  };

  if (scan_storage_.prev_raw.size() != count * stride) {
    LogWarning("Mismatch in previous raw data size. Aborting NextScan.");
    return;
  }

  // --- Optimization: Use SIMD for NextScan ---
  if (scan_comparison_ == ScanComparison::kExactValue) {
    auto callback = [&](size_t offset) {
      if (offset % stride != 0) {
        return;
      }
      const size_t index = offset / stride;
      if (index >= count) {
        return;
      }
      if (index < success_mask.size() && success_mask[index] == 0) {
        return;
      }

      filtered_storage.addresses.emplace_back(scan_storage_.addresses[index]);
      const auto val_start = current_memory_buffer.begin() + offset;
      filtered_storage.curr_raw.insert(
          filtered_storage.curr_raw.end(), val_start, val_start + stride);
      filtered_storage.prev_raw.insert(
          filtered_storage.prev_raw.end(), val_start, val_start + stride);
    };

    if (target_scan_mask_.empty()) {
      LogInfo("NextScan: ExactValue (Scalar/SIMD) stride={}", stride);
      core::ScanBuffer(
          current_memory_buffer, target_scan_value_, stride, callback);
    } else {
      LogInfo("NextScan: ExactValue (Masked SIMD) stride={}", stride);
      core::ScanBufferMasked(current_memory_buffer,
                             target_scan_value_,
                             target_scan_mask_,
                             callback);
    }
  } else if (scan_comparison_ == ScanComparison::kChanged ||
             scan_comparison_ == ScanComparison::kUnchanged) {
    const bool find_equal = (scan_comparison_ == ScanComparison::kUnchanged);
    core::ScanMemCmp(
        current_memory_buffer,
        scan_storage_.prev_raw,
        find_equal,
        stride,
        [&](size_t offset) {
          const size_t index = offset / stride;
          if (index >= count) {
            return;
          }
          if (index < success_mask.size() && success_mask[index] == 0) {
            return;
          }

          filtered_storage.addresses.emplace_back(
              scan_storage_.addresses[index]);

          const auto val_start = current_memory_buffer.begin() + offset;

          filtered_storage.curr_raw.insert(
              filtered_storage.curr_raw.end(), val_start, val_start + stride);

          // Update baseline
          filtered_storage.prev_raw.insert(
              filtered_storage.prev_raw.end(), val_start, val_start + stride);
        });
  } else if ((scan_comparison_ == ScanComparison::kIncreased ||
              scan_comparison_ == ScanComparison::kDecreased) &&
             (scan_value_type_ == ScanValueType::kInt32 ||
              scan_value_type_ == ScanValueType::kFloat)) {
    // --- Optimization: Use SIMD for Increased/Decreased (Int32/Float only) ---
    const bool greater = (scan_comparison_ == ScanComparison::kIncreased);

    auto callback = [&](size_t offset) {
      const size_t index = offset / stride;
      if (index >= count) {
        return;
      }
      if (index < success_mask.size() && success_mask[index] == 0) {
        return;
      }

      filtered_storage.addresses.emplace_back(scan_storage_.addresses[index]);
      const auto val_start = current_memory_buffer.begin() + offset;
      filtered_storage.curr_raw.insert(
          filtered_storage.curr_raw.end(), val_start, val_start + stride);
      filtered_storage.prev_raw.insert(
          filtered_storage.prev_raw.end(), val_start, val_start + stride);
    };

    if (scan_value_type_ == ScanValueType::kInt32) {
      if (greater) {
        core::ScanMemCompareGreater<int32_t>(
            current_memory_buffer, scan_storage_.prev_raw, callback);
      } else {
        core::ScanMemCompareGreater<int32_t>(
            scan_storage_.prev_raw, current_memory_buffer, callback);
      }
    } else {
      if (greater) {
        core::ScanMemCompareGreater<float>(
            current_memory_buffer, scan_storage_.prev_raw, callback);
      } else {
        core::ScanMemCompareGreater<float>(
            scan_storage_.prev_raw, current_memory_buffer, callback);
      }
    }
  } else {
    // --- Standard Scalar Loop for other comparisons ---
    PerformNextScanScalar(current_memory_buffer,
                          scan_storage_,
                          count,
                          stride,
                          filtered_storage,
                          check_condition);
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

  // We assert now to avoid issues if the list size changed but the buffer
  // did not catch up yet. If for any reason this happens, this should be fixed.
  const size_t required_size =
      scan_storage_.addresses.size() * scan_storage_.stride;
  Assert(scan_storage_.curr_raw.size() == required_size);

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
  modules_ = process->GetModules();
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
