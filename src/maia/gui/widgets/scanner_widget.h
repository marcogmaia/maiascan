// Copyright (c) Maia

#pragma once

#include <any>

#include <imgui.h>
#include <entt/signal/sigh.hpp>

#include "maia/application/scan_result_model.h"
#include "maia/logging.h"

namespace maia::gui {

class ScannerWidget {
 public:
  class Signals {
   public:
    entt::sigh<void()> scan_button_pressed;
  };

  Signals signals;

  void Render() const {
    if (ImGui::Begin("Mapped regions")) {
      if (ImGui::Button("Scan")) {
        signals.scan_button_pressed.publish();
      }

      if (ImGui::BeginTable("Tab", 2)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Value");

        ImGui::TableHeadersRow();
        for (const auto& entry : entries_) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(std::format("{}", entry.address).c_str());
          ImGui::TextUnformatted(
              std::format("{}", *static_cast<uint8_t*>(entry.data)).c_str());
        }
        ImGui::EndTable();
      }
    }
    ImGui::End();
  }

  void SetMemory(std::vector<ScanEntry> entries) {
    LogInfo("Memory set");
    entries_.swap(entries);
  }

 private:
  // A "Scan" is the result of a scanning operation, it contains the memory view
  // scanned for.
  std::vector<ScanEntry> entries_;
};

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

inline void FreeFunc() {
  LogWarning("ui");
}

class ScanPresenter {
 public:
  ScanPresenter(ScanResultModel& model, ScannerWidget& widget) {
    // clang-format off
    sinks_.Connect<&ScanResultModel::FirstScan>(widget.signals.scan_button_pressed, model)
          .Connect<&ScannerWidget::SetMemory>(model.signals().memory_changed, widget);
    // clang-format on
  }

 private:
  void OnScanPressed() {
    LogInfo("Scan pressed.");
  }

  SinkStorage sinks_;
};

}  // namespace maia::gui
