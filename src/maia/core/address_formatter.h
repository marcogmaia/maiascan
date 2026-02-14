// Copyright (c) Maia

#pragma once

#include <string>
#include <vector>

#include "maia/mmem/mmem.h"

namespace maia {

struct FormattedAddress {
  std::string text;
  bool is_relative;
};

class AddressFormatter {
 public:
  explicit AddressFormatter(std::vector<mmem::ModuleDescriptor> modules);

  FormattedAddress Format(uintptr_t address) const;

 private:
  std::vector<mmem::ModuleDescriptor> modules_;
};

}  // namespace maia
