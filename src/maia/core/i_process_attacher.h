// Copyright (c) Maia

#include <memory>

#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"

namespace maia::core {

// virtual ProcessHandle GetHandle() = 0;

class IProcessAttacher {
 public:
  virtual ~IProcessAttacher() = default;

  virtual std::unique_ptr<IProcess> AttachTo(Pid pid) = 0;
};

}  // namespace maia::core
