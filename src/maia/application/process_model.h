// Copyright (c) Maia

#pragma once

#include "maia/core/i_process_attacher.h"

#include <entt/signal/sigh.hpp>

namespace maia {

class ProcessModel {
 public:
  struct Signals {
    entt::sigh<void(IProcess*)> active_process_changed;
  };

  explicit ProcessModel(IProcessAttacher& process_attacher)
      : process_attacher_(process_attacher) {}

  // Return true in case the attach was successful.
  bool AttachToProcess(Pid pid) {
    active_process_ = process_attacher_.AttachTo(pid);
    if (!active_process_) {
      return false;
    }
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
  IProcessAttacher& process_attacher_;
  std::unique_ptr<IProcess> active_process_;
};

}  // namespace maia
