// Copyright (c) Maia

#pragma once

#include <any>

#include <imgui.h>
#include <entt/signal/sigh.hpp>

#include "maia/logging.h"
#include "maia/scanner/memory_common.h"

namespace maia::gui {

struct MemoryEntry {
  MemoryAddress address;
  MemoryPtr data;
  // Size of the data, this should be changed to an enum.
  size_t size;
};

class ScanModel {
 public:
  class Signals {
   public:
    entt::sigh<void(std::vector<MemoryEntry>)> memory_changed;
  };

  Signals signals;

 private:
  std::vector<MemoryEntry> entries_;
};

class ScanTableWidget {
 public:
  class Signals {
   public:
    entt::sigh<void()> scan_button_pressed;
  };

  Signals signals;

  explicit ScanTableWidget(ScanModel& scan_model)
      : scan_(scan_model) {}

  void Render() const {
    if (ImGui::Begin("Mapped regions")) {
      if (ImGui::Button("Scan")) {
        signals.scan_button_pressed.publish();
      }
    }
    ImGui::End();
  }

  Signals& GetEvents() {
    return events_;
  }

  void SetMemory(std::vector<MemoryEntry> entries) {
    LogInfo("Memory set");
    entries_.swap(entries);
  }

 private:
  void OnButtonPressed() {
    LogInfo("ButtonPressed.");
  }

  // A "Scan" is the result of a scanning operation, it contains the memory view
  // scanned for.
  ScanModel scan_;
  std::vector<MemoryEntry> entries_;

  class Signals events_;
};

class SinkStorage {
 public:
  template <auto U>
  void Connect(auto& sig, auto& obj) {
    auto sink = entt::sink(sig);
    sink.template connect<U>(&obj);
    sinks_.emplace_back(std::move(sink));
  }

  template <auto U>
  void Connect(auto& sig) {
    auto sink = entt::sink(sig);
    sink.template connect<U>();
    sinks_.emplace_back(std::move(sink));
  }

 private:
  std::vector<std::any> sinks_;
};

inline void FreeFunc() {
  LogWarn("ui");
}

class ScanTablePresenter {
 public:
  ScanTablePresenter(ScanModel& model, ScanTableWidget& view) {
    // clang-format off
    sinks_.Connect<[]{LogInfo("oi");}>(view.signals.scan_button_pressed);
    sinks_.Connect<FreeFunc>(view.signals.scan_button_pressed);
    sinks_.Connect<&ScanTablePresenter::OnScanPressed>(view.signals.scan_button_pressed, *this);
    sinks_.Connect<&ScanTableWidget::SetMemory>(model.signals.memory_changed, view);
    // clang-format on
  }

 private:
  void OnScanPressed() {
    LogInfo("Scan pressed.");
  }

  SinkStorage sinks_;
};

}  // namespace maia::gui
