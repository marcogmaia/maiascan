// Copyright (c) Maia

#include "maia/application/pointer_scanner_model.h"

#include <chrono>

#include "maia/logging.h"

namespace maia {

namespace {

bool CanScan(IProcess* process) {
  return (process != nullptr) && process->IsProcessValid();
}

/// \brief Operation types that can conflict with each other.
enum class OperationType {
  kGenerateMap,
  kScan,
  kValidate,
};

/// \brief Checks if any operation is blocking a new operation from starting.
/// \param generating_map Whether map generation is in progress.
/// \param scanning Whether scanning is in progress.
/// \param validating Whether validation is in progress.
/// \return The blocking operation type, or nullopt if none.
std::optional<OperationType> GetBlockingOperation(bool generating_map,
                                                  bool scanning,
                                                  bool validating) {
  if (generating_map) {
    return OperationType::kGenerateMap;
  }
  if (scanning) {
    return OperationType::kScan;
  }
  if (validating) {
    return OperationType::kValidate;
  }
  return std::nullopt;
}

/// \brief Template helper to handle pending async result processing.
/// \tparam T The result type stored in the future.
/// \tparam Handler Callable type for processing the result.
/// \param is_active Atomic flag indicating if operation is active.
/// \param future The future containing the result.
/// \param handler Callback to process the result when ready.
/// \return True if a result was processed, false otherwise.
template <typename T, typename Handler>
bool ProcessPendingResult(std::atomic<bool>& is_active,
                          std::future<T>& future,
                          Handler&& handler) {
  if (!is_active.load() || !future.valid()) {
    return false;
  }

  if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return false;
  }

  auto result = future.get();
  is_active = false;
  handler(std::move(result));
  return true;
}

/// \brief Returns a human-readable name for an operation type.
const char* GetOperationName(OperationType op) {
  switch (op) {
    case OperationType::kGenerateMap:
      return "map generation";
    case OperationType::kScan:
      return "scanning";
    case OperationType::kValidate:
      return "validation";
  }
  return "unknown operation";
}

}  // namespace

PointerScannerModel::PointerScannerModel() = default;

PointerScannerModel::~PointerScannerModel() {
  try {
    CancelOperation();
    WaitForOperation();
  } catch (const std::exception& e) {
    LogError("PointerScannerModel destructor failed: {}", e.what());
  } catch (...) {
    LogError("PointerScannerModel destructor failed with unknown error");
  }
}

void PointerScannerModel::SetTargetAddress(uint64_t address) {
  target_address_.store(address);
  LogInfo("Pointer scan target address set to: 0x{:X}", address);
}

void PointerScannerModel::SetTargetType(ScanValueType type) {
  target_type_.store(type);
}

void PointerScannerModel::SetActiveProcess(IProcess* process) {
  // If busy, cancel and wait to prevent use-after-free
  if (IsBusy()) {
    CancelOperation();
    WaitForOperation();
  }

  std::scoped_lock lock(mutex_);
  if (!CanScan(process)) {
    LogWarning("Invalid process selected for pointer scanner.");
    active_process_ = nullptr;
    modules_.clear();
    return;
  }

  active_process_ = process;
  modules_ = process->GetModules();
  LogInfo("Pointer scanner active process changed: {}",
          process->GetProcessName());
}

void PointerScannerModel::GeneratePointerMap() {
  std::scoped_lock lock(mutex_);

  // CRITICAL: Check if any operation is busy before starting
  if (auto blocking = GetBlockingOperation(is_generating_map_.load(),
                                           is_scanning_.load(),
                                           is_validating_.load())) {
    if (*blocking == OperationType::kGenerateMap) {
      LogWarning("Already generating pointer map.");
    } else {
      LogWarning("Cannot generate map while {} is in progress.",
                 GetOperationName(*blocking));
      signals_.map_generated.publish(false, 0);
    }
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Cannot generate pointer map: no valid process.");
    signals_.map_generated.publish(false, 0);
    return;
  }

  is_generating_map_ = true;

  LogInfo("Starting pointer map generation...");
  progress_ = 0.0f;
  map_progress_ = 0.0f;
  current_operation_ = "Generating Pointer Map";
  stop_source_ = std::stop_source{};
  this->cancelled_ = false;
  this->is_cancelling_ = false;

  pending_map_ = std::async(
      std::launch::async,
      [this,
       process = active_process_,
       stop_token = stop_source_.get_token()]() {
        // Check if cancelled immediately
        if (this->cancelled_.load()) {
          this->is_cancelling_ = false;
          return std::optional<core::PointerMap>{};
        }
        auto result =
            core::PointerMap::Generate(*process, stop_token, [this](float p) {
              map_progress_ = p * 0.9f;  // Reserve 10% for finalization
            });
        this->is_cancelling_ = false;
        return result;
      });
}

bool PointerScannerModel::SaveMap(const std::filesystem::path& path) const {
  std::scoped_lock lock(mutex_);
  if (!pointer_map_.has_value()) {
    LogWarning("Cannot save: no pointer map generated.");
    return false;
  }
  return pointer_map_->Save(path);
}

bool PointerScannerModel::LoadMap(const std::filesystem::path& path) {
  std::scoped_lock lock(mutex_);

  if (is_generating_map_.load() || is_scanning_.load()) {
    LogWarning("Cannot load map while operation in progress.");
    return false;
  }

  auto loaded = core::PointerMap::Load(path);
  if (!loaded) {
    LogWarning("Failed to load pointer map from: {}", path.string());
    return false;
  }

  pointer_map_ = std::move(*loaded);
  LogInfo("Loaded pointer map with {} entries from: {}",
          pointer_map_->GetEntryCount(),
          path.string());
  signals_.map_generated.publish(true, pointer_map_->GetEntryCount());
  return true;
}

void PointerScannerModel::FindPaths(const core::PointerScanConfig& config) {
  std::scoped_lock lock(mutex_);

  // CRITICAL: Check if any operation is busy before starting
  if (auto blocking = GetBlockingOperation(is_generating_map_.load(),
                                           is_scanning_.load(),
                                           is_validating_.load())) {
    if (*blocking == OperationType::kScan) {
      LogWarning("Already scanning for paths.");
    } else {
      LogWarning("Cannot scan while {} is in progress.",
                 GetOperationName(*blocking));
      signals_.scan_complete.publish(core::PointerScanResult{
          .success = false,
          .error_message = fmt::format("Cannot scan while {} is in progress.",
                                       GetOperationName(*blocking))});
    }
    return;
  }

  if (!pointer_map_.has_value()) {
    LogWarning("Cannot scan: no pointer map available.");
    signals_.scan_complete.publish(core::PointerScanResult{
        .success = false,
        .error_message = "No pointer map generated. Click 'Generate' first."});
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Cannot scan: no valid process.");
    signals_.scan_complete.publish(core::PointerScanResult{
        .success = false, .error_message = "No valid process attached."});
    return;
  }

  is_scanning_ = true;

  LogInfo("Starting pointer scan for target: 0x{:X}...", config.target_address);
  progress_ = 0.0f;
  scan_progress_ = 0.0f;
  current_operation_ = "Finding Pointer Paths";
  stop_source_ = std::stop_source{};
  this->cancelled_ = false;
  this->is_cancelling_ = false;

  core::PointerScanner scanner;
  pending_scan_ = scanner.FindPathsAsync(
      pointer_map_.value(),
      config,
      modules_,
      stop_source_.get_token(),
      [this](float p) { scan_progress_ = 0.1f + p * 0.9f; });
}

std::vector<core::PointerPath> PointerScannerModel::ValidatePaths() const {
  std::scoped_lock lock(mutex_);

  if (!CanScan(active_process_)) {
    LogWarning("Cannot validate: no valid process.");
    return {};
  }

  if (paths_.empty()) {
    LogInfo("No paths to validate.");
    return {};
  }

  const uint64_t target = target_address_.load();
  LogInfo(
      "Validating {} paths against target: 0x{:X}...", paths_.size(), target);

  core::PointerScanner scanner;
  auto valid_paths = scanner.FilterPaths(*active_process_, paths_, target);

  LogInfo("Validation complete: {} of {} paths are still valid.",
          valid_paths.size(),
          paths_.size());
  return valid_paths;
}

void PointerScannerModel::ValidatePathsAsync() {
  std::scoped_lock lock(mutex_);

  // CRITICAL: Check if any operation is busy before starting
  if (auto blocking = GetBlockingOperation(is_generating_map_.load(),
                                           is_scanning_.load(),
                                           is_validating_.load())) {
    if (*blocking == OperationType::kValidate) {
      LogWarning("Already validating paths.");
    } else {
      LogWarning("Cannot validate while {} is in progress.",
                 GetOperationName(*blocking));
      signals_.validation_complete.publish({});
    }
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Cannot validate: no valid process.");
    signals_.validation_complete.publish({});
    return;
  }

  if (paths_.empty()) {
    LogInfo("No paths to validate.");
    signals_.validation_complete.publish({});
    return;
  }

  is_validating_ = true;

  // Capture necessary state under the lock
  IProcess* process = active_process_;
  std::vector<core::PointerPath> paths_to_validate = paths_;
  const uint64_t target = target_address_.load();
  this->cancelled_ = false;
  this->is_cancelling_ = false;

  LogInfo("Starting async validation of {} paths...", paths_to_validate.size());

  pending_validation_ = std::async(
      std::launch::async,
      [this, process, paths = std::move(paths_to_validate), target]() {
        // Check if cancelled immediately
        if (this->cancelled_.load()) {
          this->is_cancelling_ = false;
          return std::vector<core::PointerPath>{};
        }
        // CRITICAL FIX: Check process validity RIGHT BEFORE using it
        // to prevent use-after-free if process was destroyed
        if (!process || !process->IsProcessValid()) {
          LogWarning("Validation cancelled: process invalid or destroyed.");
          this->is_cancelling_ = false;
          return std::vector<core::PointerPath>{};
        }
        core::PointerScanner scanner;
        auto result = scanner.FilterPaths(*process, paths, target);
        this->is_cancelling_ = false;
        return result;
      });
}

void PointerScannerModel::CancelOperation() {
  if (is_generating_map_.load() || is_scanning_.load() ||
      is_validating_.load()) {
    LogInfo("Cancelling pointer scanner operation...");
    this->cancelled_ = true;
    this->is_cancelling_ = true;
    stop_source_.request_stop();
  }
}

void PointerScannerModel::WaitForOperation() {
  if (pending_map_.valid()) {
    pending_map_.wait();
  }
  if (pending_scan_.valid()) {
    pending_scan_.wait();
  }
  if (pending_validation_.valid()) {
    pending_validation_.wait();
  }
}

void PointerScannerModel::SetPaths(
    const std::vector<core::PointerPath>& paths) {
  std::scoped_lock lock(mutex_);
  paths_ = paths;
  signals_.paths_updated.publish();
  LogInfo("Pointer paths updated: {} paths", paths_.size());
}

void PointerScannerModel::Clear() {
  std::scoped_lock lock(mutex_);
  paths_.clear();
  signals_.paths_updated.publish();
  LogInfo("Pointer scan results cleared.");
}

std::vector<std::string> PointerScannerModel::GetModuleNames() const {
  std::scoped_lock lock(mutex_);
  std::vector<std::string> names;
  names.reserve(modules_.size());
  for (const auto& mod : modules_) {
    names.push_back(mod.name);
  }
  return names;
}

std::optional<uint64_t> PointerScannerModel::ResolvePath(
    const core::PointerPath& path) const {
  std::scoped_lock lock(mutex_);
  if (!active_process_ || !active_process_->IsProcessValid()) {
    return std::nullopt;
  }

  core::PointerScanner scanner;
  return scanner.ResolvePath(*active_process_, path, modules_);
}

}  // namespace maia
