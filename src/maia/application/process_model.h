// Copyright (c) Maia

/// \file process_model.h
/// \brief Manages the lifecycle of the active target process.
///
/// \details
/// **Role**: The single source of truth for "which process is currently being
/// hacked."
///
/// **Architecture**:
///    - **Event Hub**: Centralizes process attachment/detachment logic.
///    - **Observer Pattern**: Emits the `ActiveProcessChanged` signal via EnTT.
///
/// **Thread Safety**:
///    - Not inherently thread-safe. Should primarily be accessed from the
///    main/UI thread.
///
/// **Key Interactions**:
///    - **Driven by**: `ProcessSelectorPresenter`.
///    - **Listened to by**: `ScanResultModel`, `CheatTableModel`,
///    `PointerScannerModel`.
///      When this model changes the active process, all other models reset
///      their state.

#pragma once

#include "maia/core/process.h"

#include <entt/signal/sigh.hpp>

namespace maia {

class ProcessModel {
 public:
  struct Signals {
    entt::sigh<void(IProcess*)> active_process_changed;
    entt::sigh<void()> process_will_detach;
  };

  struct Sinks {
    ProcessModel& model;

    auto ActiveProcessChanged() {
      return entt::sink(model.signals_.active_process_changed);
    }

    auto ProcessWillDetach() {
      return entt::sink(model.signals_.process_will_detach);
    }
  };

  Sinks sinks() {
    return Sinks{*this};
  }

  /// \brief Return true in case the attach was successful.
  bool AttachToProcess(Pid pid) {
    auto proc = Process::Create(pid);
    if (!proc) {
      return false;
    }

    active_process_ = std::make_unique<Process>(std::move(*proc));
    signals_.active_process_changed.publish(active_process_.get());
    return true;
  }

  /// \brief Detaches from the current process and clears state.
  void Detach() {
    signals_.process_will_detach.publish();
    active_process_.reset();
    signals_.active_process_changed.publish(nullptr);
  }

  /// \brief Returns a raw pointer to the active process.
  IProcess* GetActiveProcess() const {
    return active_process_.get();
  }

  /// \brief Explicitly sets the active process (primarily for testing).
  void SetActiveProcess(std::unique_ptr<IProcess> process) {
    active_process_ = std::move(process);
    signals_.active_process_changed.publish(active_process_.get());
  }

  Signals& signals() {
    return signals_;
  }

 private:
  Signals signals_;
  std::unique_ptr<IProcess> active_process_;
};

}  // namespace maia
