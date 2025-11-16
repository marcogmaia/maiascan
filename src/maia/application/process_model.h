// Copyright (c) Maia

#pragma once

#include "maia/core/process.h"

#include <entt/signal/sigh.hpp>

namespace maia {

class ProcessModel {
 public:
  struct Signals {
    entt::sigh<void(IProcess*)> active_process_changed;
  };

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
