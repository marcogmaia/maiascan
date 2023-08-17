#pragma once

// #include <cstddef>
#include <algorithm>
#include <vector>

#include <tl/optional.hpp>

#include "maiascan/scanner/match.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/types.h"

namespace maia {

namespace detail {

inline Match::Offsets SearchOffsets(const Bytes &haystack, const Bytes &needle) {
  Match::Offsets offsets;

  for (auto it = haystack.cbegin();
       (it = std::search(it, haystack.cend(), needle.cbegin(), needle.cend())) != haystack.cend();
       std::advance(it, 1)) {
    const auto offset = std::distance(haystack.cbegin(), it);
    offsets.push_back(offset);
  }

  return offsets;
}

}  // namespace detail

// std::span
inline tl::optional<Matches> Search(Process &proc, const Bytes &bytes) {
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

}  // namespace maia
