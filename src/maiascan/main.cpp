#include <afx.h>

#include <iostream>

#include <fmt/core.h>
#include <cxxopts.hpp>

#include "maiascan/scanner/scanner.h"

int main(int argc, const char* const* argv) {
  cxxopts::Options opts("maiascan", "Memory scanner");
  opts.add_options("help")("h,help", "Show help", cxxopts::value<bool>()->default_value("false"));
  opts.add_options()("l,list", "List processes");

  try {
    auto result = opts.parse(argc, argv);
    if (result.arguments().empty()) {
      std::cout << opts.help();
      return 1;
    }
    if (result["h"].as<bool>()) {
      std::cout << opts.help();
      return 0;
    }

    if (result["l"].as<bool>()) {
      maia::ListProcs();
    }
  } catch (cxxopts::exceptions::parsing& e) {
    std::cout << fmt::format("Failed to parse: {}\n", e.what());
    std::cout << opts.help();
  }

  return 0;
}
