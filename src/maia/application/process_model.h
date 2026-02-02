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
  };

  struct Sinks {
    ProcessModel& model;

    // clang-format off
    auto ActiveProcessChanged() { return entt::sink(model.signals_.active_process_changed); };

    // clang-format on
  };

  Sinks sinks() {
    return Sinks{*this};
  }

  // Return true in case the attach was successful.
  bool AttachToProcess(Pid pid) {
    auto proc = Process::Create(pid);
    if (!proc) {
      return false;
    }

    active_process_ = std::make_unique<Process>(std::move(*proc));
    signals_.active_process_changed.publish(active_process_.get());
    return true;
  }

  void Detach() {
    active_process_.reset();
    signals_.active_process_changed.publish(nullptr);
  }

  Signals& signals() {
    return signals_;
  }

 private:
  Signals signals_;
  std::unique_ptr<IProcess> active_process_;
};

}  // namespace maia
