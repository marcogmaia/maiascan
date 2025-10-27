
#pragma once

#include <string>
#include <variant>

namespace maia::console {

struct CommandAttach {
  std::string process_name;
};

using Command = std::variant<CommandAttach>;

}  // namespace maia::console
