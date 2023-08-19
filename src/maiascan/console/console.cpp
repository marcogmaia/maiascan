
#include <CLI/CLI.hpp>

#include "maiascan/console/commands.h"
#include "maiascan/console/console.h"

namespace maia ::console {

namespace {}

struct Console::Impl {};

Console::Console() : impl_(nullptr) {}

tl::expected<Command, std::string> Parse(const std::string& command) {
  CLI::App app("maiascan");

  bool fl;
  app.add_flag("-p,--print", fl, "Print configuration and exit");

  int pid;
  app.add_option("-a,--attach", pid, "Process pid to attach");

  try {
    app.parse(command);
  } catch (const CLI ::ParseError& e) {
    return tl::unexpected(e.what());
  };

  return CommandAttach{pid};
}

}  // namespace maia::console
