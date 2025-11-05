// Copyright (c) Maia

#include <fmt/core.h>
#include <CLI/CLI.hpp>

#include "maia/console/commands.h"
#include "maia/console/console.h"

namespace maia ::console {

struct Console::Impl {};

Console::Console()
    : impl_(nullptr) {}

std::expected<Command, std::string> Parse(const char* const* argv,
                                          int argc,
                                          bool skip_first) {
  std::string command{};
  for (int i = skip_first ? 1 : 0; i < argc; ++i) {
    command += " ";
    command += argv[i];
  }
  return Parse(command);
}

std::expected<Command, std::string> Parse(const std::string& command) {
  CLI::App app("maiascan");

  bool fl;
  app.add_flag("-p,--print", fl, "Print configuration and exit");

  std::string process_name;
  app.add_option("-a,--attach", process_name, "Name of the process to attach");

  try {
    app.parse(command);
  } catch (const CLI::ParseError& e) {
    return std::unexpected(app.help());
  };

  return CommandAttach{process_name};
}

}  // namespace maia::console
