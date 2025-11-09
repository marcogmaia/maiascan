// Copyright (c) Maia

#pragma once

#include <source_location>
#include <string_view>

#include <libassert/assert.hpp>

namespace maia {

void Assert(bool assertion,
            std::string_view message,
            std::source_location sloc) {
  ASSERT(assertion, message, sloc);
  std::unreachable();
}

void Assert(bool assertion, std::source_location sloc) {
  ASSERT(assertion, sloc);
  std::unreachable();
}

}  // namespace maia
