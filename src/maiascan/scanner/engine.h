#pragma once

#include <algorithm>
#include <cstddef>
#include <regex>
#include <span>
#include <vector>

#include <fmt/core.h>
#include <tl/optional.hpp>

#include "maiascan/scanner/match.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scanner.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

tl::optional<Pid> GetPidFromProcessName(const std::string &proc_name);

tl::optional<Matches> Search(Process &proc, BytesView bytes);

std::vector<MemoryAddress> GetAddressMatches(const Matches &matches);

inline MemoryAddress NextAddress(MemoryAddress address, int64_t diff = 1) {
  return static_cast<MemoryAddress>(std::next(static_cast<uint8_t *>(address), diff));
};

template <typename Callable>
void ForEachMatchesAddress(const Matches &matches, Callable &&func) {
  for (const auto &match : matches) {
    for (const auto &offset : match.offsets) {
      func(NextAddress(match.page.address, offset));
    }
  }
}

}  // namespace maia::scanner
