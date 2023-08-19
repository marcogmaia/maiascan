#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include <tl/optional.hpp>

#include "maiascan/scanner/match.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/types.h"

namespace maia::scanner {

namespace detail {

inline Match::Offsets SearchOffsets(BytesView haystack, BytesView needle, int align = 4) {
  Match::Offsets offsets;

  for (auto it = haystack.begin();
       (it = std::search(it, haystack.end(), needle.begin(), needle.end())) != haystack.end();
       std::advance(it, align)) {
    const auto offset = std::distance(haystack.begin(), it);
    offsets.push_back(offset);
  }

  return offsets;
}

}  // namespace detail

// std::span
inline tl::optional<Matches> Search(Process &proc, BytesView bytes) {
  const auto &pages = proc.QueryPages();
  Matches matches;
  matches.reserve(pages.size());

  for (const auto &page : pages) {
    auto memory = proc.ReadPage(page);
    if (!memory) {
      continue;
    }
    auto offsets = detail::SearchOffsets(*memory, bytes);
    matches.emplace_back(page, offsets);
  }
  matches.shrink_to_fit();
  return matches;
}

std::vector<MemoryAddress> GetAddressMatches(const Matches &matches);

}  // namespace maia::scanner
