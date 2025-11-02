// Copyright (c) Maia

#pragma once

#include "maia/core/i_process.h"

namespace maia {

class ProcessModel {
 public:
  explicit ProcessModel(IProcess& process)
      : process_(process) {}

 private:
  IProcess& process_;
};

}  // namespace maia
