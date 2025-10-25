// Copyright (c) Maia

#pragma once

#include <memory>

#include <expected>

#include "maiascan/console/commands.h"

namespace maia::console {

// Desired API:
// Parse(string) -> Command;
// Run(Command);
std::expected<Command, std::string> Parse(const std::string& command);

std::expected<Command, std::string> Parse(const char* const* argv,
                                          int argc,
                                          bool skip_first);

class Console {
 public:
  Console();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace maia::console
