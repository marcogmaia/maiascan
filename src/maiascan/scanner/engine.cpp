#include "maiascan/scanner/engine.h"

namespace maia::scanner {

std::vector<MemoryAddress> GetAddressMatches(const Matches &matches) {
  int total_offsets = 0;
  for (const auto &match : matches) {
    total_offsets += match.offsets.size();
  }

  std::vector<MemoryAddress> addresses;
  addresses.reserve(total_offsets);
  for (const auto &match : matches) {
    for (const auto &offset : match.offsets) {
      addresses.emplace_back(match.page.address + offset);
    }
  }
  return addresses;
}

}  // namespace maia::scanner
