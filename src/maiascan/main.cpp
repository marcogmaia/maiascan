#include <afx.h>

#include <bit>
#include <iostream>
#include <variant>

#include <fmt/core.h>

#include "maiascan/console/console.h"
#include "maiascan/scanner/engine.h"
#include "maiascan/scanner/process.h"
#include "maiascan/scanner/scanner.h"

namespace maia::scanner {

namespace {

template <typename T>
BytesView ToBytesView(T data) {
  constexpr bool kIsCString = std::is_same_v<char *, std::decay_t<T>> || std::is_same_v<const char *, std::decay_t<T>>;
  if constexpr (kIsCString) {
    return BytesView(std::bit_cast<std::byte *>(data), strlen(data));
  }
  return BytesView(std::bit_cast<std::byte *>(&data), sizeof(T));
}

template <typename T>
auto SearchT(Process &proc, T needle) {
  return Search(proc, ToBytesView(needle));
}

std::vector<MemoryAddress> GetAddressMatches(const Matches &matches) {
  int total_offsets = 0;
  for (const auto &match : matches) {
    total_offsets += match.offsets.size();
  }

  std::vector<MemoryAddress> addresses;
  addresses.reserve(total_offsets);
  for (const auto &match : matches) {
    for (const auto &offset : match.offsets) {
      addresses.emplace_back(NextAddress(match.page.address, offset));
    }
  }
  return addresses;
}

// void GetChangedAddresses(Matches matches>)

void FilterOutChangedAddresses() {}

template <typename T>
std::vector<T> ReadAllValues(const Process &proc, const std::vector<MemoryAddress> &addresses) {
  std::vector<T> values;
  values.reserve(addresses.size());

  for (const auto &addr : addresses) {
    T buffer{};
    auto res = proc.ReadIntoBuffer(addr, ToBytesView(buffer));
    if (!res) {
      std::cout << res.error() << std::endl;
    } else {
      values.emplace_back(buffer);
    }
  }

  return values;
}

}  // namespace

}  // namespace maia::scanner

int main(int argc, const char *const *argv) {
  std::string command{};
  for (int i = 1; i < argc; ++i) {
    command += " ";
    command += argv[i];
  }
  auto res = maia::console::Parse(command);
  if (!res) {
    std::cout << res.error() << '\n';
    return 1;
  }

  try {
    std::visit(
        [](maia::console::CommandAttach &command) {
          std::cout << fmt::format("selected pid: {}\n", command.pid);
          maia::scanner::Process proc{static_cast<maia::Pid>(command.pid)};
          int needle = 1337;
          auto scan = maia::scanner::SearchT(proc, needle);
          if (scan) {
            auto addresses = maia::scanner::GetAddressMatches(*scan);
            auto values = maia::scanner::ReadAllValues<decltype(needle)>(proc, addresses);
            // for (const auto *addr : addresses) {
            //   std::cout << fmt::format("{:p}\n", std::bit_cast<const void *>(addr));
            // }
          }
        },
        *res);
  } catch (std::exception &) {
  }

  return 0;
}
