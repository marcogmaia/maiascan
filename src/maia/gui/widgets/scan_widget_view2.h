// Copyright (c) Maia

#pragma once

#include <cstdio>  // For puts/printf in MVVM demo
#include <string>
#include <vector>

#include <imgui.h>
#include <entt/signal/sigh.hpp>  // EnTT signal/slot header

namespace maia::gui {

/// @brief A simple structure to hold a memory scan result.
struct ScanResult {
  std::string address;
  std::string value;
};

// --- 1. The State (Part of the ViewModel) ---
/// @brief Holds the complete UI state for the memory scanner widget.
/// This is the "state" part of the ViewModel.
struct ScannerWidgetState {
  // --- Window State ---
  bool is_window_open_ = true;

  // --- Scan Results ---
  std::vector<ScanResult> found_addresses = {
      {.address = "GameAsse...", .value = "F3 44 0F 10 93 30 01 00 00"},
      {.address = "0x1C8A4F...", .value = "F3 0F 11 73 08 48 8B 5C 24"},
      {.address = "0x1C8A53...", .value = "48 8B 5C 24 30 48 83 C4 20"}
  };
  int selected_result_index = -1;

  // --- Scan Options State (Data-bound by the View) ---
  bool hex_checked = true;
  char value_buffer[256] = "?? ?? ?? 02 45 33 C9 41 0F";
  int scan_type_current = 0;
  int value_type_current = 0;
  char start_addr[65] = "0000000000000000";
  char stop_addr[65] = "00007fffffffffff";
  bool opt_writable = false;
  bool opt_executable = true;
  bool opt_fast_scan = true;
  bool opt_unrandomizer = false;
  bool opt_speedhack = false;

  // --- Static Dropdown Data ---
  static inline const char* scan_types[] = {"Search for this array",
                                            "Exact Value",
                                            "Bigger than...",
                                            "Smaller than..."};
  static inline const int scan_types_count = IM_ARRAYSIZE(scan_types);

  static inline const char* value_types[] = {"Array of byte",
                                             "Byte",
                                             "2 Bytes",
                                             "4 Bytes",
                                             "8 Bytes",
                                             "Float",
                                             "Double"};
  static inline const int value_types_count = IM_ARRAYSIZE(value_types);
};

// --- 2. The View ---
/// @brief Renders the Memory Scanner UI based on the State and emits signals
/// (Commands) on interaction.
class ScannerWidgetView {
 public:
  /// @brief Renders the complete Memory Scanner widget window.
  /// @param state The state to be rendered.
  /// @param p_open A pointer to the boolean controlling window visibility.
  void Render(ScannerWidgetState& state, bool* p_open) {
    ImGui::Begin("Memory Scanner", p_open);

    if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn(
          "LeftPane", ImGuiTableColumnFlags_WidthFixed, 200.0f);
      ImGui::TableSetupColumn("RightPane", ImGuiTableColumnFlags_WidthStretch);

      // --- Column 1: Left Panel (Scan Results) ---
      ImGui::TableNextColumn();
      RenderLeftPane(state);

      // --- Column 2: Right Panel (Scan Options) ---
      ImGui::TableNextColumn();
      RenderRightPane(state);

      ImGui::EndTable();
    }
    ImGui::End();
  }

  // --- Sinks (for the ViewModel to connect its Commands) ---
  entt::sink<entt::sigh<void()>> new_scan_requested_sink{new_scan_requested_};
  entt::sink<entt::sigh<void()>> next_scan_requested_sink{next_scan_requested_};
  entt::sink<entt::sigh<void()>> undo_scan_requested_sink{undo_scan_requested_};
  entt::sink<entt::sigh<void()>> memory_view_requested_sink{
      memory_view_requested_};
  entt::sink<entt::sigh<void(int)>> result_selected_sink{result_selected_};

 private:
  void RenderLeftPane(ScannerWidgetState& state) {
    ImGui::Text("Found: %zu", state.found_addresses.size());

    float button_height = ImGui::GetTextLineHeightWithSpacing() +
                          ImGui::GetStyle().FramePadding.y * 2.0f;
    ImGui::BeginChild("AddressListChild", ImVec2(0, -button_height), true);

    if (ImGui::BeginTable("AddressList",
                          2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY)) {
      ImGui::TableSetupColumn("Address");
      ImGui::TableSetupColumn("Value");
      ImGui::TableHeadersRow();

      for (int i = 0; i < state.found_addresses.size(); ++i) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        bool is_selected = (state.selected_result_index == i);
        if (ImGui::Selectable(state.found_addresses[i].address.c_str(),
                              is_selected,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          state.selected_result_index = i;
          // Publish signal (like invoking a command)
          result_selected_.publish(i);
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(state.found_addresses[i].value.c_str());
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();

    if (ImGui::Button("Memory View", ImVec2(-1, 0))) {
      // Publish signal (invoke command)
      memory_view_requested_.publish();
    }
  }

  void RenderRightPane(ScannerWidgetState& state) {
    ImGui::BeginChild("ScannerOptionsChild");

    if (ImGui::Button("New Scan")) {
      new_scan_requested_.publish();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Scan")) {
      next_scan_requested_.publish();
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo Scan")) {
      undo_scan_requested_.publish();
    }

    // --- Direct Data Binding (the "ImGui Way") ---
    ImGui::Checkbox("Hex", &state.hex_checked);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
    ImGui::InputText(
        "Value", state.value_buffer, IM_ARRAYSIZE(state.value_buffer));

    ImGui::SetNextItemWidth(200);
    ImGui::Combo("Scan Type",
                 &state.scan_type_current,
                 ScannerWidgetState::scan_types,
                 ScannerWidgetState::scan_types_count);
    ImGui::SetNextItemWidth(200);
    ImGui::Combo("Value Type",
                 &state.value_type_current,
                 ScannerWidgetState::value_types,
                 ScannerWidgetState::value_types_count);

    if (ImGui::CollapsingHeader("Memory Scan Options")) {
      ImGui::Indent();
      ImGui::Text("Start");
      ImGui::InputText("##StartAddr",
                       state.start_addr,
                       IM_ARRAYSIZE(state.start_addr),
                       ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::Text("Stop");
      ImGui::InputText("##StopAddr",
                       state.stop_addr,
                       IM_ARRAYSIZE(state.stop_addr),
                       ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::Checkbox("Writable", &state.opt_writable);
      ImGui::SameLine(120);
      ImGui::Checkbox("Executable", &state.opt_executable);
      ImGui::Checkbox("Fast Scan", &state.opt_fast_scan);
      ImGui::Unindent();
    }

    ImGui::Spacing();
    float right_align_pos = ImGui::GetWindowWidth() - 200;
    if (right_align_pos > 0) {
      ImGui::SameLine(right_align_pos);
    }

    ImGui::BeginChild("OptionsRight", ImVec2(180, 50), false);
    ImGui::Checkbox("Unrandomizer", &state.opt_unrandomizer);
    ImGui::Checkbox("Enable Speedhack", &state.opt_speedhack);
    ImGui::EndChild();

    ImGui::EndChild();
  }

  // --- Signals (Commands invoked by the View) ---
  entt::sigh<void()> new_scan_requested_;
  entt::sigh<void()> next_scan_requested_;
  entt::sigh<void()> undo_scan_requested_;
  entt::sigh<void()> memory_view_requested_;
  entt::sigh<void(int)> result_selected_;
};

// --- 3. The ViewModel ---
/// @brief Connects the View to the State and handles application logic.
/// This is the main class to interact with.
class ScannerWidgetViewModel {
 public:
  /// @brief Connects signals from the View to local "Command" handlers.
  ScannerWidgetViewModel() {
    view_.new_scan_requested_sink.connect<&ScannerWidgetViewModel::OnNewScan>(
        this);
    view_.next_scan_requested_sink.connect<&ScannerWidgetViewModel::OnNextScan>(
        this);
    view_.undo_scan_requested_sink.connect<&ScannerWidgetViewModel::OnUndoScan>(
        this);
    view_.memory_view_requested_sink
        .connect<&ScannerWidgetViewModel::OnMemoryView>(this);
    view_.result_selected_sink
        .connect<&ScannerWidgetViewModel::OnResultSelected>(this);
  }

  /// @brief Renders the widget. This is the main entry point for the UI
  /// loop.
  void Render() {
    // Only render if the window is supposed to be open.
    // The view's Render() will update state_.is_window_open_ if the user
    // clicks 'x'.
    if (state_.is_window_open_) {
      view_.Render(state_, &state_.is_window_open_);
    }
  }

  // --- Public API to control the widget ---
  void Show() {
    state_.is_window_open_ = true;
  }

  void Hide() {
    state_.is_window_open_ = false;
  }

  bool IsOpen() const {
    return state_.is_window_open_;
  }

 private:
  // --- "Command" Handlers (Application Logic) ---

  void OnNewScan() {
    // This is where you would trigger the actual memory scan logic.
    // For this demo, we'll just print and update the state.
    puts("ViewModel: 'New Scan' command executed.");

    // Logic updates the State
    state_.found_addresses.clear();
    state_.found_addresses.push_back(
        {"0xNEW...", "Scan based on: " + std::string(state_.value_buffer)});
    state_.selected_result_index = -1;
    // The View will automatically reflect this change on the next frame.
  }

  void OnNextScan() {
    puts("ViewModel: 'Next Scan' command executed.");
    // Logic for "next scan" would go here.
  }

  void OnUndoScan() {
    puts("ViewModel: 'Undo Scan' command executed.");
    // Logic for "undo scan" would go here.
  }

  void OnMemoryView() {
    if (state_.selected_result_index != -1) {
      // Logic to show a memory view window for the selected address
      printf("ViewModel: 'Memory View' command for index %d (%s)\n",
             state_.selected_result_index,
             state_.found_addresses.at(state_.selected_result_index)
                 .address.c_str());
    } else {
      puts("ViewModel: 'Memory View' command, but nothing selected.");
    }
  }

  void OnResultSelected(int index) {
    // Logic to run when a new result is selected
    printf("ViewModel: Result %d selected.\n", index);
    // e.g., this could update a different widget with details
  }

  // --- Member Data ---
  ScannerWidgetState state_;
  ScannerWidgetView view_;
};

}  // namespace maia::gui

/*
---------------------------------------------------------------------------
HOW TO USE THIS IN YOUR main() / APPLICATION LOOP:
---------------------------------------------------------------------------

#include "YourNewHeader.h" // This file
#include <imgui_impl_...h> // Your ImGui backend headers

// In your application's initialization:
static maia::gui::ScannerWidgetViewModel g_scanner_view_model;

// In your main application loop (pseudocode):
while (!done)
{
    // Start new ImGui frame
    ImGui_Impl_..._NewFrame();
    ImGui::NewFrame();

    // --- Your Other UI ---
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            // Checkbox to control window visibility
            bool is_open = g_scanner_view_model.IsOpen();
            if (ImGui::MenuItem("Memory Scanner", "CTRL+M", &is_open)) {
                 is_open ? g_scanner_view_model.Show()
                         : g_scanner_view_model.Hide();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- Render the scanner ---
    // This one call handles all rendering and logic for the widget
    g_scanner_view_model.Render();

    ImGui::ShowDemoWindow(); // etc.

    // End ImGui frame and render
    ImGui::Render();
    // Render draw data
    ImGui_Impl_..._RenderDrawData(ImGui::GetDrawData());
}

---------------------------------------------------------------------------
*/
