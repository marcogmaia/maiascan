// Copyright (c) Maia

#pragma once

#include "maia/core/task_runner.h"

namespace maia::test {

/// \brief A task runner that does nothing.
/// \details Useful for tests where the logic loop is effectively infinite,
/// and we want to prevent it from starting so we can drive it manually.
class NoOpTaskRunner : public core::ITaskRunner {
 public:
  void Run(std::function<void(std::stop_token)>) override {
    // Do nothing.
  }

  void RequestStop() override {}

  void Join() override {}
};

}  // namespace maia::test
