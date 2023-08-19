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

}  // namespace maia::scanner
