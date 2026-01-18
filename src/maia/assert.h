// Copyright (c) Maia

#pragma once

#include <source_location>
#include <string_view>

namespace maia {

void Assert(bool assertion,
            std::string_view message,
            std::source_location sloc = std::source_location::current());

void Assert(bool assertion,
            std::source_location sloc = std::source_location::current());

}  // namespace maia
