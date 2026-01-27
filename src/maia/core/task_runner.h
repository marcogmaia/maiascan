// Copyright (c) Maia

#pragma once

#include <functional>
#include <stop_token>
#include <thread>

namespace maia::core {

/// \brief Abstract interface for running long-running tasks.
/// Allows swapping between async (threaded) execution and synchronous execution
/// for testing.
class ITaskRunner {
 public:
  virtual ~ITaskRunner() = default;

  /// \brief Runs the given task.
  virtual void Run(std::function<void(std::stop_token)> task) = 0;

  /// \brief Requests the running task to stop.
  virtual void RequestStop() = 0;

  /// \brief Waits for the task to complete.
  virtual void Join() = 0;
};

/// \brief Runs tasks in a background thread (std::jthread).
class AsyncTaskRunner : public ITaskRunner {
 public:
  void Run(std::function<void(std::stop_token)> task) override {
    thread_ = std::jthread(std::move(task));
  }

  void RequestStop() override {
    thread_.request_stop();
  }

  void Join() override {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  std::jthread thread_;
};

/// \brief Runs tasks immediately on the calling thread.
/// Useful for deterministic unit testing.
class SyncTaskRunner : public ITaskRunner {
 public:
  void Run(std::function<void(std::stop_token)> task) override {
    // Create a stop source that is never requested, unless we want to test
    // cancellation. For simple sync tests, a default token is usually fine.
    std::stop_source source;
    task(source.get_token());
  }

  void RequestStop() override {
    // Cannot stop a synchronous task from the outside once it has started
    // (since Run blocks until completion).
  }

  void Join() override {
    // No-op
  }
};

}  // namespace maia::core
