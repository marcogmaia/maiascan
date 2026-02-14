// Copyright (c) Maia

#include "maia/core/scanner.h"

#include <algorithm>
#include <array>
#include <future>
#include <span>
#include <thread>
#include <vector>

#include "maia/core/scoped_process_suspend.h"
#include "maia/core/simd_scanner.h"
#include "maia/logging.h"

namespace maia::core {

namespace {

struct ScanTask {
  uintptr_t base_address;
  size_t scan_size;
  size_t read_size;
};

constexpr bool IsReadable(mmem::Protection prot) noexcept {
  const auto prot_val = static_cast<uint32_t>(prot);
  const auto read_val = static_cast<uint32_t>(mmem::Protection::kRead);
  return (prot_val & read_val) != 0;
}

constexpr size_t GetDataTypeSize(ScanValueType type) {
  switch (type) {
    case ScanValueType::kUInt8:
    case ScanValueType::kInt8:
      return 1;
    case ScanValueType::kUInt16:
    case ScanValueType::kInt16:
      return 2;
    case ScanValueType::kUInt32:
    case ScanValueType::kInt32:
    case ScanValueType::kFloat:
      return 4;
    case ScanValueType::kUInt64:
    case ScanValueType::kInt64:
    case ScanValueType::kDouble:
      return 8;
    case ScanValueType::kString:
    case ScanValueType::kWString:
    case ScanValueType::kArrayOfBytes:
      return 1;
    default:
      return 4;
  }
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr T LoadValue(std::span<const std::byte> bytes) {
  std::array<std::byte, sizeof(T)> buffer{};
  const size_t copy_size = std::min(bytes.size(), sizeof(T));
  std::ranges::copy_n(bytes.begin(), copy_size, buffer.begin());
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

template <CScanPredicate Func>
constexpr auto DispatchScanType(ScanValueType type, Func&& func) {
  switch (type) {
    case ScanValueType::kUInt8:
      return func.template operator()<uint8_t>();
    case ScanValueType::kUInt16:
      return func.template operator()<uint16_t>();
    case ScanValueType::kUInt32:
      return func.template operator()<uint32_t>();
    case ScanValueType::kUInt64:
      return func.template operator()<uint64_t>();
    case ScanValueType::kInt8:
      return func.template operator()<int8_t>();
    case ScanValueType::kInt16:
      return func.template operator()<int16_t>();
    case ScanValueType::kInt32:
      return func.template operator()<int32_t>();
    case ScanValueType::kInt64:
      return func.template operator()<int64_t>();
    case ScanValueType::kFloat:
      return func.template operator()<float>();
    case ScanValueType::kDouble:
      return func.template operator()<double>();
    default:
      return false;
  }
}

}  // namespace

ScanResult Scanner::FirstScan(IProcess& process,
                              const ScanConfig& config,
                              std::stop_token stop_token,
                              ProgressCallback progress_callback) const {
  ScanResult result;

  if (!config.Validate()) {
    result.error_message = "Invalid scan configuration";
    return result;
  }

  if (!process.IsProcessValid()) {
    result.error_message = "Process is not valid";
    return result;
  }

  std::optional<ScopedProcessSuspend> suspend;
  if (config.pause_while_scanning) {
    suspend.emplace(&process);
  }

  const bool is_exact_scan = (config.comparison == ScanComparison::kExactValue);
  size_t scan_stride = 0;

  if (is_exact_scan) {
    if (config.value.empty()) {
      result.error_message = "Exact value scan requires a value";
      return result;
    }
    scan_stride = config.value.size();
  } else {
    scan_stride = GetDataTypeSize(config.value_type);
  }

  if (scan_stride == 0) {
    result.error_message = "Invalid scan stride";
    return result;
  }

  const size_t alignment = config.alignment;

  ScanStorage storage;
  storage.stride = scan_stride;
  storage.value_type = config.value_type;

  auto regions = process.GetMemoryRegions();
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
    result.success = true;
    return result;
  }

  std::atomic<size_t> processed_tasks{0};

  const size_t num_threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::vector<ScanTask>> thread_batches(num_threads);
  for (size_t i = 0; i < total_tasks; ++i) {
    thread_batches[i % num_threads].emplace_back(tasks[i]);
  }

  auto worker =
      [&process,
       is_exact_scan,
       scan_stride,
       alignment,
       &config,
       &stop_token,
       &processed_tasks,
       total_tasks,
       &progress_callback](const std::vector<ScanTask>& batch) -> ScanStorage {
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
      if (!process.ReadMemory({&addr, 1}, task.read_size, buffer, nullptr)) {
        ++processed_tasks;
        if (progress_callback) {
          progress_callback(static_cast<float>(processed_tasks) /
                            static_cast<float>(total_tasks));
        }
        continue;
      }

      if (is_exact_scan) {
        auto callback = [&](size_t offset) {
          if (offset >= task.scan_size) {
            return;
          }
          local_storage.addresses.emplace_back(task.base_address + offset);
          local_storage.curr_raw.insert(local_storage.curr_raw.end(),
                                        buffer.begin() + offset,
                                        buffer.begin() + offset + scan_stride);
        };

        if (config.mask.empty()) {
          ScanBuffer(buffer, config.value, alignment, callback);
        } else {
          ScanBufferMasked(buffer, config.value, config.mask, callback);
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

      ++processed_tasks;
      if (progress_callback) {
        progress_callback(static_cast<float>(processed_tasks) /
                          static_cast<float>(total_tasks));
      }
    }
    return local_storage;
  };

  std::vector<std::future<ScanStorage>> futures;
  for (const auto& batch : thread_batches) {
    if (!batch.empty()) {
      futures.emplace_back(std::async(std::launch::async, worker, batch));
    }
  }

  // Collect all partial results first.
  std::vector<ScanStorage> partial_results;
  partial_results.reserve(futures.size());
  for (auto& future : futures) {
    if (stop_token.stop_requested()) {
      break;
    }
    partial_results.emplace_back(future.get());
  }

  if (stop_token.stop_requested()) {
    result.error_message = "Scan cancelled";
    return result;
  }

  // Calculate total size and reserve memory upfront to avoid reallocations.
  size_t total_addresses = 0;
  size_t total_raw_bytes = 0;
  for (const auto& partial : partial_results) {
    total_addresses += partial.addresses.size();
    total_raw_bytes += partial.curr_raw.size();
  }

  storage.addresses.reserve(total_addresses);
  storage.curr_raw.reserve(total_raw_bytes);

  // Merge all partial results into the final storage.
  for (auto& partial : partial_results) {
    storage.addresses.insert(storage.addresses.end(),
                             std::make_move_iterator(partial.addresses.begin()),
                             std::make_move_iterator(partial.addresses.end()));
    storage.curr_raw.insert(storage.curr_raw.end(),
                            std::make_move_iterator(partial.curr_raw.begin()),
                            std::make_move_iterator(partial.curr_raw.end()));
  }

  storage.prev_raw = storage.curr_raw;
  result.storage = std::move(storage);
  result.success = true;

  LogInfo("FirstScan complete. Found {} addresses.",
          result.storage.addresses.size());

  return result;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
ScanResult Scanner::NextScan(IProcess& process,
                             const ScanConfig& config,
                             const ScanStorage& previous_results,
                             std::stop_token stop_token,
                             ProgressCallback progress_callback) const {
  ScanResult result;

  if (!config.Validate()) {
    result.error_message = "Invalid scan configuration";
    return result;
  }

  if (!process.IsProcessValid()) {
    result.error_message = "Process is not valid";
    return result;
  }

  if (previous_results.addresses.empty()) {
    result.error_message = "No previous results to filter";
    return result;
  }

  std::optional<ScopedProcessSuspend> suspend;
  if (config.pause_while_scanning) {
    suspend.emplace(&process);
  }

  const size_t count = previous_results.addresses.size();
  const size_t prev_stride = previous_results.stride;

  // For exact value scans, use the new value's size as the stride if provided.
  // This allows string/AoB scans to change length between scans.
  const size_t stride = (config.comparison == ScanComparison::kExactValue &&
                         !config.value.empty())
                            ? config.value.size()
                            : prev_stride;

  constexpr size_t kBatchSize = 65536;

  ScanStorage filtered_storage;
  filtered_storage.stride = stride;
  filtered_storage.value_type = previous_results.value_type;
  filtered_storage.addresses.reserve(count / 2);
  filtered_storage.curr_raw.reserve((count / 2) * stride);
  filtered_storage.prev_raw.reserve((count / 2) * stride);

  std::vector<std::byte> batch_buffer;
  batch_buffer.reserve(kBatchSize * stride);
  std::vector<uint8_t> batch_success_mask;
  batch_success_mask.reserve(kBatchSize);
  std::vector<uintptr_t> batch_addresses;
  batch_addresses.reserve(kBatchSize);

  size_t processed_count = 0;

  for (size_t batch_start = 0; batch_start < count; batch_start += kBatchSize) {
    if (stop_token.stop_requested()) {
      result.error_message = "Scan cancelled";
      return result;
    }

    const size_t batch_count = std::min(kBatchSize, count - batch_start);

    batch_addresses.assign(
        previous_results.addresses.begin() + batch_start,
        previous_results.addresses.begin() + batch_start + batch_count);

    batch_buffer.resize(batch_count * stride);
    batch_success_mask.assign(batch_count, 0);

    if (!process.ReadMemory(
            batch_addresses, stride, batch_buffer, &batch_success_mask)) {
      processed_count += batch_count;
      if (progress_callback) {
        progress_callback(static_cast<float>(processed_count) /
                          static_cast<float>(count));
      }
      continue;
    }

    std::span<const std::byte> prev_span(
        previous_results.prev_raw.data() + (batch_start * prev_stride),
        batch_count * prev_stride);

    const auto check_condition = [&](std::span<const std::byte> curr,
                                     std::span<const std::byte> prev,
                                     size_t relative_index) -> bool {
      if (relative_index < batch_success_mask.size() &&
          batch_success_mask[relative_index] == 0) {
        return false;
      }

      switch (config.comparison) {
        case ScanComparison::kChanged:
          return IsValueChanged(curr, prev);
        case ScanComparison::kUnchanged:
          return !IsValueChanged(curr, prev);

        case ScanComparison::kIncreased:
          return DispatchScanType(config.value_type, [&]<typename T>() {
            return IsValueIncreased<T>(curr, prev);
          });

        case ScanComparison::kDecreased:
          return DispatchScanType(config.value_type, [&]<typename T>() {
            return IsValueDecreased<T>(curr, prev);
          });

        case ScanComparison::kIncreasedBy:
          return DispatchScanType(config.value_type, [&]<typename T>() {
            return IsValueIncreasedBy<T>(curr, prev, std::span{config.value});
          });

        case ScanComparison::kDecreasedBy:
          return DispatchScanType(config.value_type, [&]<typename T>() {
            return IsValueDecreasedBy<T>(curr, prev, std::span{config.value});
          });

        case ScanComparison::kExactValue:
          return std::ranges::equal(curr, config.value);

        default:
          return false;
      }
    };

    auto callback = [&](size_t offset) {
      if (offset % stride != 0) {
        return;
      }
      const size_t relative_index = offset / stride;
      if (relative_index >= batch_count) {
        return;
      }
      if (relative_index < batch_success_mask.size() &&
          batch_success_mask[relative_index] == 0) {
        return;
      }

      const size_t absolute_index = batch_start + relative_index;
      filtered_storage.addresses.emplace_back(
          previous_results.addresses[absolute_index]);

      const auto val_start = batch_buffer.begin() + offset;
      filtered_storage.curr_raw.insert(
          filtered_storage.curr_raw.end(), val_start, val_start + stride);
      filtered_storage.prev_raw.insert(
          filtered_storage.prev_raw.end(), val_start, val_start + stride);
    };

    if (config.comparison == ScanComparison::kExactValue) {
      if (config.mask.empty()) {
        ScanBuffer(batch_buffer, config.value, stride, callback);
      } else {
        ScanBufferMasked(batch_buffer, config.value, config.mask, callback);
      }
    } else if (config.comparison == ScanComparison::kChanged ||
               config.comparison == ScanComparison::kUnchanged) {
      const bool find_equal = (config.comparison == ScanComparison::kUnchanged);
      ScanMemCmp(batch_buffer, prev_span, find_equal, stride, callback);
    } else if (config.comparison == ScanComparison::kIncreased ||
               config.comparison == ScanComparison::kDecreased) {
      const bool greater = (config.comparison == ScanComparison::kIncreased);
      DispatchScanType(config.value_type, [&]<typename T>() {
        if (greater) {
          ScanMemCompareGreater<T>(batch_buffer, prev_span, callback);
        } else {
          ScanMemCompareGreater<T>(prev_span, batch_buffer, callback);
        }
        return true;
      });
    } else {
      const std::byte* curr_ptr = batch_buffer.data();
      const std::byte* prev_ptr = prev_span.data();

      for (size_t i = 0; i < batch_count; ++i) {
        std::span<const std::byte> val_curr(curr_ptr, stride);
        std::span<const std::byte> val_prev(prev_ptr, prev_stride);

        if (check_condition(val_curr, val_prev, i)) {
          filtered_storage.addresses.emplace_back(
              previous_results.addresses[batch_start + i]);
          filtered_storage.curr_raw.insert(filtered_storage.curr_raw.end(),
                                           val_curr.begin(),
                                           val_curr.end());
          // Update previous value to current for next scan
          filtered_storage.prev_raw.insert(filtered_storage.prev_raw.end(),
                                           val_curr.begin(),
                                           val_curr.end());
        }

        curr_ptr += stride;
        prev_ptr += prev_stride;
      }
    }

    processed_count += batch_count;
    if (progress_callback) {
      progress_callback(static_cast<float>(processed_count) /
                        static_cast<float>(count));
    }
  }

  result.storage = std::move(filtered_storage);
  result.success = true;

  LogInfo("NextScan complete. {} addresses remaining.",
          result.storage.addresses.size());

  return result;
}

std::future<ScanResult> Scanner::FirstScanAsync(
    IProcess& process,
    const ScanConfig& config,
    std::stop_token stop_token,
    ProgressCallback progress_callback) const {
  return std::async(
      std::launch::async,
      [this, &process, config, stop_token, progress_callback]() noexcept {
        try {
          return FirstScan(process, config, stop_token, progress_callback);
        } catch (const std::exception& e) {
          ScanResult result;
          result.error_message = e.what();
          return result;
        } catch (...) {
          ScanResult result;
          result.error_message = "Unknown exception occurred";
          return result;
        }
      });
}

std::future<ScanResult> Scanner::NextScanAsync(
    IProcess& process,
    const ScanConfig& config,
    const ScanStorage& previous_results,
    std::stop_token stop_token,
    ProgressCallback progress_callback) const {
  return std::async(
      std::launch::async,
      [this,
       &process,
       config,
       previous_results,
       stop_token,
       progress_callback]() noexcept {
        try {
          return NextScan(
              process, config, previous_results, stop_token, progress_callback);
        } catch (const std::exception& e) {
          ScanResult result;
          result.error_message = e.what();
          return result;
        } catch (...) {
          ScanResult result;
          result.error_message = "Unknown exception occurred";
          return result;
        }
      });
}

}  // namespace maia::core
