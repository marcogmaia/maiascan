#pragma once

#include <memory>

#include <tl/expected.hpp>

#include "maiascan/console/commands.h"

namespace maia::console {

// Desired API:
// Parse(string) -> Command;
// Run(Command);
tl::expected<Command, std::string> Parse(const std::string &command);

class Console {
 public:
  Console();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace maia::console
