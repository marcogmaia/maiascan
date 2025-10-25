// Copyright (c) Maia

#include <regex>

#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/scanner.h"

namespace maia::scanner {

namespace detail {

Match::Offsets SearchOffsets(BytesView haystack,
                             BytesView needle,
                             int align = 4) {
  Match::Offsets offsets;

  for (auto it = haystack.begin();
       (it = std::search(it, haystack.end(), needle.begin(), needle.end())) !=
       haystack.end();
       std::advance(it, align)) {
    const auto offset = std::distance(haystack.begin(), it);
    offsets.push_back(offset);
  }

  return offsets;
}

}  // namespace detail

std::vector<MemoryAddress> GetAddressMatches(const Matches& matches) {
  int total_offsets = 0;
  for (const auto& match : matches) {
    total_offsets += match.offsets.size();
  }

  std::vector<MemoryAddress> addresses;
  addresses.reserve(total_offsets);
  for (const auto& match : matches) {
    for (const auto& offset : match.offsets) {
      addresses.emplace_back(NextAddress(match.page.address, offset));
    }
  }
  return addresses;
}

std::optional<Pid> GetPidFromProcessName(const std::string& proc_name) {
  if (proc_name.empty()) {
    return std::nullopt;
  }
  std::regex pattern{fmt::format("^{}.*", proc_name),
                     std::regex_constants::icase};
  std::smatch match{};
  auto procs = GetProcs();
  for (const auto& proc : procs) {
    if (std::regex_match(proc.name, match, pattern)) {
      return proc.pid;
    }
  }
  return std::nullopt;
}

std::optional<Matches> Search(Process& proc, BytesView bytes) {
  const auto& pages = proc.QueryPages();
  Matches matches;
  matches.reserve(pages.size());

  for (const auto& page : pages) {
    // TODO(marco): remove ReadPage from here.
    if (auto memory = proc.ReadPage(page)) {
      auto offsets = detail::SearchOffsets(*memory, bytes);
      matches.emplace_back(page, offsets);
    }
  }
  matches.shrink_to_fit();
  return matches;
}

}  // namespace maia::scanner
