// Copyright (c) Maia

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "maia/core/memory_common.h"

namespace maia {

class IProcess;

struct ParsedAddress {
  /// \brief The final resolved address.
  /// If a process was provided and the input involved a module, this is the
  /// actual memory address (base + offset).
  /// If no process was provided but the input was purely arithmetic, this is
  /// the result. If the input involved a module but no process was provided,
  /// this might be 0 or partial.
  MemoryAddress resolved_address{0};

  /// \brief The name of the module, if the input was module-relative.
  std::string module_name;

  /// \brief The offset from the module base.
  /// If module_name is set, this is the offset.
  /// If module_name is empty, this is usually 0 (as the address is absolute).
  uint64_t module_offset{0};
};

/// \brief Parses an address expression string.
/// \param input The string to parse (e.g., "0x1234", "game.exe+0x100",
/// "0x100+10").
/// \param process Optional pointer to the active process for module lookup.
/// \return A ParsedAddress struct if parsing was successful, std::nullopt
/// otherwise.
std::optional<ParsedAddress> ParseAddressExpression(std::string_view input,
                                                    const IProcess* process);

}  // namespace maia
