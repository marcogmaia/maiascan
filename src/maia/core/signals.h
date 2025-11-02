// Copyright (c) Maia

#pragma once

#include <any>
#include <vector>

#include <entt/signal/sigh.hpp>

namespace maia {

class SinkStorage {
 public:
  template <auto U>
  auto& Connect(auto& sig, auto& obj) {
    auto sink = entt::sink(sig);
    sink.template connect<U>(&obj);
    sinks_.emplace_back(std::move(sink));
    return *this;
  }

  template <auto U>
  auto& Connect(auto& sig) {
    auto sink = entt::sink(sig);
    sink.template connect<U>();
    sinks_.emplace_back(std::move(sink));
    return *this;
  }

 private:
  std::vector<std::any> sinks_;
};

}  // namespace maia
