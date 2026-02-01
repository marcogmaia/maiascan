// Copyright (c) Maia

#include "maia/core/address_parser.h"

#include <algorithm>
#include <vector>

#include "maia/core/i_process.h"
#include "maia/core/string_utils.h"
#include "maia/core/value_parser.h"

namespace maia {

std::optional<ParsedAddress> ParseAddressExpression(std::string_view input,
                                                    const IProcess* process) {
  // Simple parser: supports "TERM + TERM + ..." or "TERM - TERM"
  // Currently we'll just handle addition for simplicity as that's the main
  // case.
  // TODO: Full expression parser if needed.

  auto parts = core::Split(input, '+');
  if (parts.empty()) {
    return std::nullopt;
  }

  ParsedAddress result;

  // Analyze the first term
  std::string_view first_term = core::Trim(parts[0]);

  // Check if first term is a module
  bool first_is_module = false;
  uint64_t base_addr = 0;

  auto number_opt = core::ParseNumber<uint64_t>(first_term, 0);
  if (number_opt) {
    base_addr = *number_opt;
  } else {
    // Not a number, assume module name?
    // Only if it ends in typical extensions or we can find it in process
    if (process) {
      auto modules = process->GetModules();
      std::string term_str(first_term);
      // Case insensitive comparison would be better, but exact match first
      auto it = std::find_if(modules.begin(),
                             modules.end(),
                             [&](const auto& m) { return m.name == term_str; });

      if (it != modules.end()) {
        first_is_module = true;
        result.module_name = term_str;
        base_addr = it->base;
      }
    }

    // If we didn't find it in process (or no process), but it looks like a
    // module (contains dot), assume it is one.
    if (!first_is_module && first_term.find('.') != std::string_view::npos) {
      first_is_module = true;
      result.module_name = std::string(first_term);
      base_addr = 0;  // Can't resolve yet
    }

    if (!first_is_module) {
      // Not a number and not a module -> invalid
      return std::nullopt;
    }
  }

  // Sum up the rest
  uint64_t total_offset = 0;
  for (size_t i = 1; i < parts.size(); ++i) {
    auto offset_opt = core::ParseNumber<uint64_t>(parts[i], 0);
    if (!offset_opt) {
      return std::nullopt;  // All subsequent terms must be numbers
    }
    total_offset += *offset_opt;
  }

  if (first_is_module) {
    result.module_offset = total_offset;
    result.resolved_address = base_addr + total_offset;
  } else {
    result.resolved_address = base_addr + total_offset;
    // It was purely arithmetic, keep module info empty
  }

  return result;
}

}  // namespace maia
