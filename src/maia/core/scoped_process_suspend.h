// Copyright (c) Maia

#pragma once

#include "maia/core/i_process.h"

namespace maia {

/// \brief RAII helper to suspend a process and automatically resume it.
class ScopedProcessSuspend {
 public:
  explicit ScopedProcessSuspend(IProcess* process)
      : process_(process) {
    if (process_) {
      process_->Suspend();
    }
  }

  ~ScopedProcessSuspend() {
    if (process_) {
      process_->Resume();
    }
  }

  // Prevent copying
  ScopedProcessSuspend(const ScopedProcessSuspend&) = delete;
  ScopedProcessSuspend& operator=(const ScopedProcessSuspend&) = delete;

  // Allow moving
  ScopedProcessSuspend(ScopedProcessSuspend&& other) noexcept
      : process_(other.process_) {
    other.process_ = nullptr;
  }

  ScopedProcessSuspend& operator=(ScopedProcessSuspend&& other) noexcept {
    if (this != &other) {
      if (process_) {
        process_->Resume();
      }
      process_ = other.process_;
      other.process_ = nullptr;
    }
    return *this;
  }

 private:
  IProcess* process_;
};

}  // namespace maia
