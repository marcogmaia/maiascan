#include "maia/core/address_formatter.h"

#include <algorithm>
#include <format>

namespace maia {

AddressFormatter::AddressFormatter(std::vector<mmem::ModuleDescriptor> modules)
    : modules_(std::move(modules)) {
  // Sort modules by base address to allow binary search
  std::ranges::sort(
      modules_, [](const auto& a, const auto& b) { return a.base < b.base; });
}

FormattedAddress AddressFormatter::Format(uintptr_t address) const {
  // Find the first module with base > address
  auto it = std::ranges::upper_bound(
      modules_, address, {}, &mmem::ModuleDescriptor::base);

  // The module containing the address must be the one before 'it'
  if (it != modules_.begin()) {
    const auto& module = *std::prev(it);
    if (address >= module.base && address < module.end) {
      uintptr_t offset = address - module.base;
      return {.text = std::format("{}+0x{:X}", module.name, offset),
              .is_relative = true};
    }
  }

  return {.text = std::format("0x{:X}", address), .is_relative = false};
}

}  // namespace maia
