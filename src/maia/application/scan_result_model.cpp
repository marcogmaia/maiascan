// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <chrono>
#include <future>
#include <thread>

#include "maia/logging.h"

namespace maia {

namespace {

bool CanScan(IProcess* process) {
  return (process != nullptr) && process->IsProcessValid();
}

}  // namespace

ScanResultModel::ScanResultModel(size_t chunk_size)
    : session_(std::make_shared<core::ScanSession>()) {
  scanner_.SetChunkSize(chunk_size);
}

ScanResultModel::~ScanResultModel() {
  CancelScan();
  StopAutoUpdate();
}

core::ScanConfig ScanResultModel::BuildScanConfig(bool use_previous) const {
  core::ScanConfig config;
  config.value_type = scan_value_type_;
  config.comparison = scan_comparison_;
  config.value = target_scan_value_;
  config.mask = target_scan_mask_;

  size_t type_size = GetSizeForType(scan_value_type_);
  if (type_size == 0) {
    type_size = 1;
  }
  config.alignment = fast_scan_enabled_ ? type_size : 1;

  config.use_previous_results = use_previous;
  config.pause_while_scanning = pause_while_scanning_enabled_;
  return config;
}

const ScanStorage& ScanResultModel::entries() const {
  return session_->GetStorageUnsafe();
}

void ScanResultModel::FirstScan() {
  if (is_scanning_.load()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for first scan.");
    return;
  }

  session_->Clear();
  signals_.memory_changed.publish(session_->GetStorageUnsafe());

  core::ScanConfig config = BuildScanConfig(false);
  if (!config.Validate()) {
    LogWarning("Invalid scan configuration.");
    return;
  }

  LogInfo("Starting first scan...");
  is_scanning_ = true;
  progress_ = 0.0f;
  stop_source_ = std::stop_source{};
  pending_config_ = config;

  pending_scan_ = scanner_.FirstScanAsync(
      *active_process_, config, stop_source_.get_token(), [this](float p) {
        progress_ = p;
      });
}

void ScanResultModel::NextScan() {
  if (is_scanning_.load()) {
    return;
  }

  if (!CanScan(active_process_)) {
    LogWarning("Process is invalid for next scan.");
    return;
  }

  if (!session_->HasResults()) {
    LogWarning("No previous results to filter.");
    return;
  }

  core::ScanConfig config = BuildScanConfig(true);
  if (!config.Validate()) {
    LogWarning("Invalid scan configuration.");
    return;
  }

  LogInfo("Starting next scan...");
  is_scanning_ = true;
  progress_ = 0.0f;
  stop_source_ = std::stop_source{};
  pending_config_ = config;

  // Launch async immediately. The snapshot is taken on the background thread
  // to avoid blocking the UI.
  pending_scan_ = std::async(
      std::launch::async,
      [this, config, stop_token = stop_source_.get_token()]() {
        ScanStorage snapshot = session_->GetStorageSnapshot();
        return scanner_.NextScan(
            *active_process_, config, snapshot, stop_token, [this](float p) {
              progress_ = p;
            });
      });
}

bool ScanResultModel::HasPendingResult() const {
  if (!pending_scan_.valid()) {
    return false;
  }
  return pending_scan_.wait_for(std::chrono::seconds(0)) ==
         std::future_status::ready;
}

void ScanResultModel::WaitForScanToFinish() {
  if (pending_scan_.valid()) {
    pending_scan_.wait();
  }
}

void ScanResultModel::ApplyPendingResult() {
  if (!pending_scan_.valid()) {
    return;
  }

  if (pending_scan_.wait_for(std::chrono::seconds(0)) !=
      std::future_status::ready) {
    return;
  }

  core::ScanResult result = pending_scan_.get();
  is_scanning_ = false;

  if (!result.success) {
    LogWarning("Scan failed: {}", result.error_message);
    return;
  }

  session_->CommitResults(std::move(result.storage), pending_config_);
  signals_.memory_changed.publish(session_->GetStorageUnsafe());

  LogInfo("Scan complete. Found {} addresses.", session_->GetResultCount());
}

void ScanResultModel::CancelScan() {
  if (is_scanning_.load()) {
    stop_source_.request_stop();
  }
}

void ScanResultModel::UpdateCurrentValues() {
  if (!CanScan(active_process_)) {
    return;
  }

  ScanStorage snapshot = session_->GetStorageSnapshot();
  if (snapshot.addresses.empty()) {
    return;
  }

  std::vector<std::byte> new_values(snapshot.addresses.size() *
                                    snapshot.stride);
  if (!active_process_->ReadMemory(
          snapshot.addresses, snapshot.stride, new_values, nullptr)) {
    return;
  }

  session_->UpdateCurrentValues(std::move(new_values));
  signals_.memory_changed.publish(session_->GetStorageUnsafe());
}

void ScanResultModel::ChangeResultType(ScanValueType new_type) {
  if (is_scanning_.load()) {
    return;
  }

  size_t new_stride = GetSizeForType(new_type);
  if (new_stride == 0) {
    new_stride = 1;
  }

  session_->ChangeType(new_type, new_stride);
  scan_value_type_ = new_type;

  // Repopulate with new values immediately
  UpdateCurrentValues();

  // Reset the previous baseline so relative scans (Changed/Unchanged)
  // start from this point.
  session_->ResetPreviousToCurrent();
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
  session_->Clear();
  signals_.memory_changed.publish(session_->GetStorageUnsafe());
}

void ScanResultModel::StartAutoUpdate() {
  if (task_.joinable()) {
    return;
  }

  task_ = std::jthread([this](std::stop_token stop_token) {
    try {
      AutoUpdateLoop(stop_token);
    } catch (const std::exception& e) {
      LogError("Auto update loop failed: {}", e.what());
    } catch (...) {
      LogError("Auto update loop failed with unknown error");
    }
  });
}

void ScanResultModel::StopAutoUpdate() {
  if (task_.joinable()) {
    task_.request_stop();
    task_.join();
  }
}

void ScanResultModel::AutoUpdateLoop(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    size_t count = session_->GetResultCount();

    if (count > 0 && count < 10000) {
      UpdateCurrentValues();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

}  // namespace maia
