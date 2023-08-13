#include <afx.h>

#include <iostream>

#include <fmt/core.h>
#include <cxxopts.hpp>

int main(int argc, const char* const* argv) {
  cxxopts::Options opts("maiascan", "Memory scanner");
  opts.add_options("help")("h,help", "Show help", cxxopts::value<bool>()->default_value("false"));

  try {
    auto result = opts.parse(argc, argv);
    // auto show_help = result["h"].as<bool>();
    if (result.arguments().empty()) {
      std::cout << opts.help();
    }
  } catch (cxxopts::exceptions::parsing& e) {
    std::cout << fmt::format("Failed to parse: {}\n", e.what());
    std::cout << opts.help();
  }

  return 0;
}
