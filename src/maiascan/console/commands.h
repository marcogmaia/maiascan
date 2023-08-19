
#pragma once

#include <string>
#include <variant>

namespace maia::console {

struct CommandAttach {
  int pid;
};

using Command = std::variant<CommandAttach>;

}  // namespace maia::console
