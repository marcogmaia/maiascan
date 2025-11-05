// Copyright (c) Maia

#pragma once

#include "core/i_process_attacher.h"
#include "live_process_accessor.h"

namespace maia {

class ProcessAttacher : public IProcessAttacher {
 public:
  std::unique_ptr<IProcess> AttachTo(Pid pid) override {
    auto* handle = scanner::OpenHandle(pid);
    return std::make_unique<scanner::LiveProcessAccessor>(handle);
  }
};

}  // namespace maia
