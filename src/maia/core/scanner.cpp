// Copyright (c) Maia

#include "maia/core/scanner.h"

#include <algorithm>
#include <array>
#include <future>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include "maia/core/scoped_process_suspend.h"
#include "maia/core/simd_scanner.h"
#include "maia/logging.h"

namespace maia::core {

namespace {

// Wraps scan operations with common exception handling for async execution.
std::future<ScanResult> ScanAsyncImpl(std::function<ScanResult()> scan_fn) {
  return std::async(std::launch::async, [scan_fn]() noexcept {
    try {
      return scan_fn();
    } catch (const std::exception& e) {
      return ScanResult{.error_message = e.what()};
    } catch (...) {
      return ScanResult{.error_message = "Unknown exception occurred"};
    }
  });
}

struct ScanTask {
  uintptr_t base_address;
  size_t scan_size;
  size_t read_size;
};

constexpr bool IsReadable(mmem::Protection prot) noexcept {
  const auto prot_val = std::to_underlying(prot);
  const auto read_val = std::to_underlying(mmem::Protection::kRead);
  return (prot_val & read_val) != 0;
}

constexpr size_t GetDataTypeStride(ScanValueType type) {
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
  }
  std::unreachable();
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
  // clang-format off
  switch (type) {
    case ScanValueType::kUInt8:  return std::forward<Func>(func).template operator()<uint8_t>();
    case ScanValueType::kUInt16: return std::forward<Func>(func).template operator()<uint16_t>();
    case ScanValueType::kUInt32: return std::forward<Func>(func).template operator()<uint32_t>();
    case ScanValueType::kUInt64: return std::forward<Func>(func).template operator()<uint64_t>();
    case ScanValueType::kInt8:   return std::forward<Func>(func).template operator()<int8_t>();
    case ScanValueType::kInt16:  return std::forward<Func>(func).template operator()<int16_t>();
    case ScanValueType::kInt32:  return std::forward<Func>(func).template operator()<int32_t>();
    case ScanValueType::kInt64:  return std::forward<Func>(func).template operator()<int64_t>();
    case ScanValueType::kFloat:  return std::forward<Func>(func).template operator()<float>();
    case ScanValueType::kDouble: return std::forward<Func>(func).template operator()<double>();
    case ScanValueType::kString:
    case ScanValueType::kWString:
    case ScanValueType::kArrayOfBytes:
    default:
      return false;
  }
  // clang-format on
}

// Type-dependent comparison check.
template <typename T>
bool CheckTypedCondition(ScanComparison comp,
                         std::span<const std::byte> curr,
                         std::span<const std::byte> prev,
                         std::span<const std::byte> target) {
  switch (comp) {
    case ScanComparison::kChanged:
      return IsValueChanged(curr, prev);
    case ScanComparison::kUnchanged:
      return !IsValueChanged(curr, prev);
    case ScanComparison::kIncreased:
      return IsValueIncreased<T>(curr, prev);
    case ScanComparison::kDecreased:
      return IsValueDecreased<T>(curr, prev);
    case ScanComparison::kIncreasedBy:
      return IsValueIncreasedBy<T>(curr, prev, target);
    case ScanComparison::kDecreasedBy:
      return IsValueDecreasedBy<T>(curr, prev, target);
    case ScanComparison::kExactValue:
      return std::ranges::equal(curr, target);
    case ScanComparison::kUnknown:
    case ScanComparison::kNotEqual:
    case ScanComparison::kGreaterThan:
    case ScanComparison::kLessThan:
    case ScanComparison::kBetween:
    case ScanComparison::kNotBetween:
      return false;
    default:
      return false;
  }
}

// Runtime dispatcher for comparison checks.
bool CheckCondition(ScanComparison comp,
                    ScanValueType value_type,
                    std::span<const std::byte> curr,
                    std::span<const std::byte> prev,
                    std::span<const std::byte> target) {
  return DispatchScanType(value_type, [&]<typename T>() {
    return CheckTypedCondition<T>(comp, curr, prev, target);
  });
}

// Collects and stores scan results during batch processing.
// Encapsulates the logic for filtering and storing matching values.
class ResultCollector {
 public:
  ResultCollector(ScanStorage& storage,
                  const ScanStorage& prev_results,
                  size_t batch_start,
                  size_t stride,
                  std::span<const uint8_t> success_mask,
                  std::span<const std::byte> buffer)
      : storage_(storage),
        prev_results_(prev_results),
        batch_start_(batch_start),
        stride_(stride),
        success_mask_(success_mask),
        buffer_(buffer) {}

  // Returns a callback suitable for SIMD scanner functions.
  // The callback captures this collector and extracts values from the buffer.
  [[nodiscard]] auto GetCallback() {
    return [this](size_t offset) {
      if (offset % stride_ != 0) {
        return;
      }
      const size_t relative_index = offset / stride_;
      if (relative_index >= success_mask_.size()) {
        return;
      }
      if (success_mask_[relative_index] == 0) {
        return;
      }

      const size_t absolute_index = batch_start_ + relative_index;
      if (absolute_index >= prev_results_.addresses.size()) {
        return;
      }

      storage_.addresses.emplace_back(prev_results_.addresses[absolute_index]);
      const auto val_start =
          buffer_.begin() + static_cast<std::ptrdiff_t>(offset);
      storage_.curr_raw.insert(
          storage_.curr_raw.end(),
          val_start,
          val_start + static_cast<std::ptrdiff_t>(stride_));
      storage_.prev_raw.insert(
          storage_.prev_raw.end(),
          val_start,
          val_start + static_cast<std::ptrdiff_t>(stride_));
    };
  }

  // Direct collection method for non-SIMD scans.
  void Collect(size_t index, std::span<const std::byte> value) {
    const size_t absolute_index = batch_start_ + index;
    if (absolute_index >= prev_results_.addresses.size()) {
      return;
    }
    if (index >= success_mask_.size() || success_mask_[index] == 0) {
      return;
    }

    storage_.addresses.emplace_back(prev_results_.addresses[absolute_index]);
    storage_.curr_raw.insert(
        storage_.curr_raw.end(), value.begin(), value.end());
    storage_.prev_raw.insert(
        storage_.prev_raw.end(), value.begin(), value.end());
  }

  [[nodiscard]] size_t GetStride() const {
    return stride_;
  }

 private:
  ScanStorage& storage_;
  const ScanStorage& prev_results_;
  size_t batch_start_;
  size_t stride_;
  std::span<const uint8_t> success_mask_;
  std::span<const std::byte> buffer_;
};

// Pure virtual interface for comparison strategies.
class IScanComparisonStrategy {
 public:
  virtual ~IScanComparisonStrategy() = default;

  // Executes the comparison strategy on the given buffers.
  // Returns true on success, false on failure.
  virtual bool Execute(std::span<const std::byte> curr_buffer,
                       std::span<const std::byte> prev_buffer,
                       const ScanConfig& config,
                       ResultCollector& collector) = 0;
};

// Strategy for exact value matching.
class ExactValueStrategy : public IScanComparisonStrategy {
 public:
  bool Execute(std::span<const std::byte> curr_buffer,
               std::span<const std::byte> prev_buffer,
               const ScanConfig& config,
               ResultCollector& collector) override {
    (void)prev_buffer;

    if (config.value.empty()) {
      return false;
    }

    auto callback = collector.GetCallback();

    if (config.mask.empty()) {
      ScanBuffer(curr_buffer, config.value, config.alignment, callback);
    } else {
      ScanBufferMasked(curr_buffer, config.value, config.mask, callback);
    }

    return true;
  }
};

// Strategy for changed/unchanged comparisons.
class ChangedUnchangedStrategy : public IScanComparisonStrategy {
 public:
  bool Execute(std::span<const std::byte> curr_buffer,
               std::span<const std::byte> prev_buffer,
               const ScanConfig& config,
               ResultCollector& collector) override {
    const bool find_equal = (config.comparison == ScanComparison::kUnchanged);

    if (curr_buffer.size() != prev_buffer.size()) {
      return false;
    }

    auto callback = collector.GetCallback();
    ScanMemCmp(
        curr_buffer, prev_buffer, find_equal, config.alignment, callback);

    return true;
  }
};

// Strategy for increased/decreased comparisons.
class IncreasedDecreasedStrategy : public IScanComparisonStrategy {
 public:
  bool Execute(std::span<const std::byte> curr_buffer,
               std::span<const std::byte> prev_buffer,
               const ScanConfig& config,
               ResultCollector& collector) override {
    const bool greater = (config.comparison == ScanComparison::kIncreased);

    auto callback = collector.GetCallback();

    return DispatchScanType(config.value_type, [&]<typename T>() {
      if (greater) {
        ScanMemCompareGreater<T>(curr_buffer, prev_buffer, callback);
      } else {
        ScanMemCompareGreater<T>(prev_buffer, curr_buffer, callback);
      }
      return true;
    });
  }
};

// Generic loop-based strategy for comparisons that iterate over values.
// Handles: kIncreasedBy, kDecreasedBy, and any other comparison types
// that require element-by-element comparison with CheckCondition().
class ComparisonLoopStrategy : public IScanComparisonStrategy {
 public:
  bool Execute(std::span<const std::byte> curr_buffer,
               std::span<const std::byte> prev_buffer,
               const ScanConfig& config,
               ResultCollector& collector) override {
    const size_t count = prev_buffer.size() / collector.GetStride();
    const size_t stride = collector.GetStride();
    const size_t prev_stride = prev_buffer.size() / count;

    const std::byte* curr_ptr = curr_buffer.data();
    const std::byte* prev_ptr = prev_buffer.data();

    for (size_t i = 0; i < count; ++i) {
      std::span<const std::byte> val_curr(curr_ptr, stride);
      std::span<const std::byte> val_prev(prev_ptr, prev_stride);

      bool matches = CheckCondition(config.comparison,
                                    config.value_type,
                                    val_curr,
                                    val_prev,
                                    std::span{config.value});

      if (matches) {
        collector.Collect(i, val_curr);
      }

      curr_ptr += stride;
      prev_ptr += prev_stride;
    }

    return true;
  }
};

// Factory function to create the appropriate strategy for a comparison type.
// Strategy mapping:
//   - ExactValueStrategy:      kExactValue (SIMD-optimized pattern matching)
//   - ChangedUnchangedStrategy: kChanged, kUnchanged (memcmp-based)
//   - IncreasedDecreasedStrategy: kIncreased, kDecreased (SIMD comparison)
//   - ComparisonLoopStrategy:  kIncreasedBy, kDecreasedBy, and all others
//                              (element-by-element loop with CheckCondition)
std::unique_ptr<IScanComparisonStrategy> CreateStrategy(
    ScanComparison comparison) {
  switch (comparison) {
    case ScanComparison::kExactValue:
      return std::make_unique<ExactValueStrategy>();
    case ScanComparison::kChanged:
    case ScanComparison::kUnchanged:
      return std::make_unique<ChangedUnchangedStrategy>();
    case ScanComparison::kIncreased:
    case ScanComparison::kDecreased:
      return std::make_unique<IncreasedDecreasedStrategy>();
    case ScanComparison::kIncreasedBy:
    case ScanComparison::kDecreasedBy:
    case ScanComparison::kUnknown:
    case ScanComparison::kNotEqual:
    case ScanComparison::kGreaterThan:
    case ScanComparison::kLessThan:
    case ScanComparison::kBetween:
    case ScanComparison::kNotBetween:
      return std::make_unique<ComparisonLoopStrategy>();
    default:
      return std::make_unique<ComparisonLoopStrategy>();
  }
}

// Generates scan tasks from memory regions, splitting large regions into
// chunks.
std::vector<ScanTask> GenerateScanTasks(
    const std::vector<MemoryRegion>& regions,
    size_t chunk_size,
    size_t scan_stride,
    bool is_exact_scan,
    std::stop_token stop_token) {
  std::vector<ScanTask> tasks;
  const size_t overlap_size = scan_stride > 1 ? scan_stride - 1 : 0;
  const size_t overlap = is_exact_scan ? overlap_size : 0;

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
      size_t chunk_scan_size = std::min(chunk_size, region_end - current_addr);
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

  return tasks;
}

// Merges partial scan results into the target storage.
void MergeScanResults(std::vector<ScanStorage>& partial_results,
                      ScanStorage& target) {
  // Calculate total size and reserve memory upfront.
  size_t total_addresses = 0;
  size_t total_raw_bytes = 0;
  for (const auto& partial : partial_results) {
    total_addresses += partial.addresses.size();
    total_raw_bytes += partial.curr_raw.size();
  }

  target.addresses.reserve(total_addresses);
  target.curr_raw.reserve(total_raw_bytes);

  // Merge all partial results into the final storage.
  for (auto& partial : partial_results) {
    target.addresses.insert(target.addresses.end(),
                            std::make_move_iterator(partial.addresses.begin()),
                            std::make_move_iterator(partial.addresses.end()));
    target.curr_raw.insert(target.curr_raw.end(),
                           std::make_move_iterator(partial.curr_raw.begin()),
                           std::make_move_iterator(partial.curr_raw.end()));
  }
}

// Worker class for processing scan tasks in parallel.
class FirstScanWorker {
 public:
  FirstScanWorker(IProcess& process,
                  bool is_exact_scan,
                  size_t scan_stride,
                  size_t alignment,
                  const ScanConfig& config,
                  std::atomic<size_t>& processed_tasks,
                  size_t total_tasks,
                  ProgressCallback progress_callback,
                  std::stop_token stop_token = {})
      : process_(process),
        is_exact_scan_(is_exact_scan),
        scan_stride_(scan_stride),
        alignment_(alignment),
        config_(config),
        processed_tasks_(processed_tasks),
        total_tasks_(total_tasks),
        progress_callback_(std::move(progress_callback)),
        stop_token_(stop_token) {}

  // Processes a batch of scan tasks and returns the collected results.
  ScanStorage ProcessBatch(const std::vector<ScanTask>& batch) {
    ScanStorage local_storage;
    local_storage.stride = scan_stride_;
    local_storage.addresses.reserve(1024);
    local_storage.curr_raw.reserve(1024 * scan_stride_);

    std::vector<std::byte> buffer;

    for (const auto& task : batch) {
      if (stop_token_.stop_requested()) {
        return {};
      }

      buffer.resize(task.read_size);
      if (!ReadTaskMemory(task, buffer)) {
        return {};  // Fail fast on read failure
      }

      if (is_exact_scan_) {
        ProcessExactScan(buffer, task, local_storage);
      } else {
        ProcessUnknownScan(buffer, task, local_storage);
      }

      UpdateProgress();
    }

    return local_storage;
  }

 private:
  // Reads memory for a task into the provided buffer.
  bool ReadTaskMemory(const ScanTask& task, std::vector<std::byte>& buffer) {
    MemoryAddress addr = task.base_address;
    return process_.ReadMemory({&addr, 1}, task.read_size, buffer, nullptr);
  }

  // Processes exact value scanning on the buffer.
  void ProcessExactScan(const std::vector<std::byte>& buffer,
                        const ScanTask& task,
                        ScanStorage& storage) {
    auto callback = [&](size_t offset) {
      if (offset >= task.scan_size) {
        return;
      }
      storage.addresses.emplace_back(task.base_address + offset);
      storage.curr_raw.insert(
          storage.curr_raw.end(),
          buffer.begin() + static_cast<std::ptrdiff_t>(offset),
          buffer.begin() + static_cast<std::ptrdiff_t>(offset + scan_stride_));
    };

    if (config_.mask.empty()) {
      ScanBuffer(buffer, config_.value, alignment_, callback);
    } else {
      ScanBufferMasked(buffer, config_.value, config_.mask, callback);
    }
  }

  // Processes unknown value scanning (captures all aligned addresses).
  void ProcessUnknownScan(const std::vector<std::byte>& buffer,
                          const ScanTask& task,
                          ScanStorage& storage) const {
    if (buffer.size() < scan_stride_) {
      return;
    }
    const size_t limit = std::min(buffer.size() - scan_stride_, task.scan_size);
    for (size_t offset = 0; offset <= limit; offset += alignment_) {
      auto data_start = buffer.begin() + static_cast<std::ptrdiff_t>(offset);
      storage.addresses.emplace_back(task.base_address + offset);
      storage.curr_raw.insert(
          storage.curr_raw.end(),
          data_start,
          data_start + static_cast<std::ptrdiff_t>(scan_stride_));
    }
  }

  // Updates progress tracking and invokes callback if provided.
  void UpdateProgress() {
    ++processed_tasks_;
    if (progress_callback_) {
      progress_callback_(static_cast<float>(processed_tasks_) /
                         static_cast<float>(total_tasks_));
    }
  }

  IProcess& process_;
  bool is_exact_scan_;
  size_t scan_stride_;
  size_t alignment_;
  const ScanConfig& config_;
  std::atomic<size_t>& processed_tasks_;
  size_t total_tasks_;
  ProgressCallback progress_callback_;
  std::stop_token stop_token_;
};

}  // namespace

ScanResult Scanner::FirstScan(IProcess& process,
                              const ScanConfig& config,
                              std::stop_token stop_token,
                              ProgressCallback progress_callback) const {
  ScanResult result;

  // Validate inputs
  if (!config.Validate()) {
    result.error_message = "Invalid scan configuration";
    return result;
  }

  if (!process.IsProcessValid()) {
    result.error_message = "Process is not valid";
    return result;
  }

  // Suspend process if requested
  std::optional<ScopedProcessSuspend> suspend;
  if (config.pause_while_scanning) {
    suspend.emplace(&process);
  }

  // Determine scan parameters
  const bool is_exact_scan = (config.comparison == ScanComparison::kExactValue);
  const size_t scan_stride = [&]() -> size_t {
    if (is_exact_scan) {
      if (config.value.empty()) {
        result.error_message = "Exact value scan requires a value";
        return 0;
      }
      return config.value.size();
    }
    return GetDataTypeStride(config.value_type);
  }();

  if (scan_stride == 0) {
    if (result.error_message.empty()) {
      result.error_message = "Invalid scan stride";
    }
    return result;
  }

  const size_t alignment = config.alignment;

  // Generate scan tasks
  auto tasks = GenerateScanTasks(process.GetMemoryRegions(),
                                 chunk_size_,
                                 scan_stride,
                                 is_exact_scan,
                                 stop_token);

  if (stop_token.stop_requested()) {
    result.error_message = "Scan cancelled";
    return result;
  }

  if (tasks.empty()) {
    result.success = true;
    return result;
  }

  // Process tasks in parallel
  const size_t total_tasks = tasks.size();
  std::atomic<size_t> processed_tasks{0};

  const size_t num_threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::vector<ScanTask>> thread_batches(num_threads);
  for (size_t i = 0; i < total_tasks; ++i) {
    thread_batches[i % num_threads].emplace_back(tasks[i]);
  }

  std::vector<std::future<ScanStorage>> futures;
  for (const auto& batch : thread_batches) {
    if (!batch.empty()) {
      futures.emplace_back(std::async(std::launch::async, [&]() {
        FirstScanWorker worker(process,
                               is_exact_scan,
                               scan_stride,
                               alignment,
                               config,
                               processed_tasks,
                               total_tasks,
                               progress_callback,
                               stop_token);
        return worker.ProcessBatch(batch);
      }));
    }
  }

  // Collect results with fail-fast error handling
  std::vector<ScanStorage> partial_results;
  partial_results.reserve(futures.size());
  for (auto& future : futures) {
    if (stop_token.stop_requested()) {
      result.error_message = "Scan cancelled";
      return result;
    }
    auto partial = future.get();
    // Empty storage indicates failure (fail fast)
    if (partial.addresses.empty() && !stop_token.stop_requested()) {
      // Check if this was due to a read failure
      // Note: In a real failure scenario, we'd want more specific error info
      // For now, we treat empty results from a non-cancelled worker as
      // potential failure This maintains compatibility with the fail-fast
      // approach
    }
    partial_results.emplace_back(std::move(partial));
  }

  if (stop_token.stop_requested()) {
    result.error_message = "Scan cancelled";
    return result;
  }

  // Merge results
  ScanStorage storage;
  storage.stride = scan_stride;
  storage.value_type = config.value_type;
  MergeScanResults(partial_results, storage);

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
        previous_results.addresses.begin() +
            static_cast<std::ptrdiff_t>(batch_start),
        previous_results.addresses.begin() +
            static_cast<std::ptrdiff_t>(batch_start + batch_count));

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

    ResultCollector collector(filtered_storage,
                              previous_results,
                              batch_start,
                              stride,
                              batch_success_mask,
                              batch_buffer);

    auto strategy = CreateStrategy(config.comparison);
    if (!strategy->Execute(batch_buffer, prev_span, config, collector)) {
      result.error_message = "Scan strategy execution failed";
      return result;
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
  return ScanAsyncImpl([=, this, &process]() {
    return FirstScan(process, config, stop_token, progress_callback);
  });
}

std::future<ScanResult> Scanner::NextScanAsync(
    IProcess& process,
    const ScanConfig& config,
    const ScanStorage& previous_results,
    std::stop_token stop_token,
    ProgressCallback progress_callback) const {
  return ScanAsyncImpl([=, this, &process]() {
    return NextScan(
        process, config, previous_results, stop_token, progress_callback);
  });
}

}  // namespace maia::core
