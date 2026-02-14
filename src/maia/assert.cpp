// Copyright (c) Maia

#include "maia/assert.h"

#include <source_location>
#include <string_view>

#include <libassert/assert.hpp>

namespace maia {

void Assert(bool assertion,
            std::string_view message,
            std::source_location sloc) {
  ASSERT(assertion, message, sloc);
}

void Assert(bool assertion, std::source_location sloc) {
  ASSERT(assertion, sloc);
}

}  // namespace maia
