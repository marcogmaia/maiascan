// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"

namespace maia {

class ScannerWidget {
 public:
  class Signals {
   public:
    entt::sigh<void(std::vector<std::byte> value_to_scan)> scan_button_pressed;
  };

  void Render(const std::vector<ScanEntry>& entries) const;

  Signals& signals() {
    return signals_;
  }

 private:
  Signals signals_;

  mutable std::string str_;
};

}  // namespace maia
