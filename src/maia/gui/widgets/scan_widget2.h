// Copyright (c) Maia

#pragma once

#include <string>
#include <vector>

#include <imgui.h>

namespace maia::gui {

/// @brief A simple structure to hold a memory scan result.
struct ScanResult {
  std::string address;
  std::string value;
};

/// @brief Renders the complete Memory Scanner widget window, including the
/// results list.
/// @param p_open A pointer to a boolean variable that controls the window's
/// visibility. If the user clicks the 'x' button, this boolean will be set to
/// false.
inline void ShowMemoryScannerWindow(bool* p_open = nullptr) {
  // Begin the ImGui window
  ImGui::Begin("Memory Scanner", p_open);

  // --- Static variables to hold the widget's state ---

  // --- Scan Results (dummy data for demonstration) ---
  static std::vector<ScanResult> found_addresses = {
      {.address = "GameAsse...", .value = "F3 44 0F 10 93 30 01 00 00"},
      {.address = "0x1C8A4F...", .value = "F3 0F 11 73 08 48 8B 5C 24"},
      {.address = "0x1C8A53...", .value = "48 8B 5C 24 30 48 83 C4 20"}
  };
  static int selected_result_index = -1;  // For tracking selection

  // --- Scan Options State ---
  static bool hex_checked = true;
  static char value_buffer[256] = "?? ?? ?? 02 45 33 C9 41 0F";
  static const char* scan_types[] = {"Search for this array",
                                     "Exact Value",
                                     "Bigger than...",
                                     "Smaller than..."};
  static int scan_type_current = 0;
  static const char* value_types[] = {"Array of byte",
                                      "Byte",
                                      "2 Bytes",
                                      "4 Bytes",
                                      "8 Bytes",
                                      "Float",
                                      "Double"};
  static int value_type_current = 0;
  static char start_addr[65] = "0000000000000000";
  static char stop_addr[65] = "00007fffffffffff";
  static bool opt_writable = false;
  static bool opt_executable = true;
  static bool opt_fast_scan = true;
  static bool opt_unrandomizer = false;
  static bool opt_speedhack = false;
  // -----------------------------------------------------

  // --- Main Layout: Two-column Table ---
  // Use ImGuiTableFlags_Resizable to allow user to drag the divider
  if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn(
        "LeftPane", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("RightPane", ImGuiTableColumnFlags_WidthStretch);

    // --- Column 1: Left Panel (Scan Results) ---
    ImGui::TableNextColumn();
    {
      // "Found" count label
      ImGui::Text("Found: %zu", found_addresses.size());

      // Add a child window to make the list scrollable
      // Use -1 for height to fill remaining space, minus button height
      float button_height = ImGui::GetTextLineHeightWithSpacing() +
                            ImGui::GetStyle().FramePadding.y * 2.0f;
      ImGui::BeginChild("AddressListChild", ImVec2(0, -button_height), true);

      // The results table
      if (ImGui::BeginTable("AddressList",
                            2,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_ScrollY)) {
        // Setup columns
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        // Populate rows
        for (int i = 0; i < found_addresses.size(); ++i) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();

          // Make the item selectable
          bool is_selected = (selected_result_index == i);
          if (ImGui::Selectable(found_addresses[i].address.c_str(),
                                is_selected,
                                ImGuiSelectableFlags_SpanAllColumns)) {
            selected_result_index = i;
          }

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(found_addresses[i].value.c_str());
        }
        ImGui::EndTable();
      }
      ImGui::EndChild();  // End AddressListChild

      // "Memory View" button at the bottom, full width
      if (ImGui::Button("Memory View", ImVec2(-1, 0))) {
        // Logic to open memory view for selected_result_index
      }
    }

    // --- Column 2: Right Panel (Scan Options) ---
    ImGui::TableNextColumn();
    {
      // Use a child window to ensure layout isolation
      ImGui::BeginChild("ScannerOptionsChild");

      // --- Top Buttons ---
      if (ImGui::Button("New Scan")) {
        /* Your scan logic here */
      }
      ImGui::SameLine();
      if (ImGui::Button("Next Scan")) {
        /* Your next scan logic here */
      }
      ImGui::SameLine();
      if (ImGui::Button("Undo Scan")) {
        /* Your undo logic here */
      }

      // --- Value Input Area ---
      ImGui::Checkbox("Hex", &hex_checked);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
      ImGui::InputText("Value", value_buffer, IM_ARRAYSIZE(value_buffer));

      // --- Dropdown Selectors ---
      ImGui::SetNextItemWidth(200);  // Fixed width
      ImGui::Combo("Scan Type",
                   &scan_type_current,
                   scan_types,
                   IM_ARRAYSIZE(scan_types));
      ImGui::SetNextItemWidth(200);
      ImGui::Combo("Value Type",
                   &value_type_current,
                   value_types,
                   IM_ARRAYSIZE(value_types));

      // --- Collapsible Options ---
      if (ImGui::CollapsingHeader("Memory Scan Options")) {
        ImGui::Indent();
        ImGui::Text("Start");
        ImGui::InputText("##StartAddr",
                         start_addr,
                         IM_ARRAYSIZE(start_addr),
                         ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::Text("Stop");
        ImGui::InputText("##StopAddr",
                         stop_addr,
                         IM_ARRAYSIZE(stop_addr),
                         ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::Checkbox("Writable", &opt_writable);
        ImGui::SameLine(120);
        ImGui::Checkbox("Executable", &opt_executable);
        ImGui::Checkbox("Fast Scan", &opt_fast_scan);
        ImGui::Unindent();
      }

      // --- Right-aligned Checkboxes ---
      ImGui::Spacing();
      float right_align_pos =
          ImGui::GetWindowWidth() - 200;  // Adjust '200' as needed
      if (right_align_pos > 0) {
        ImGui::SameLine(right_align_pos);
      }

      ImGui::BeginChild("OptionsRight", ImVec2(180, 50), false);
      ImGui::Checkbox("Unrandomizer", &opt_unrandomizer);
      ImGui::Checkbox("Enable Speedhack", &opt_speedhack);
      ImGui::EndChild();

      ImGui::EndChild();  // End ScannerOptionsChild
    }

    ImGui::EndTable();  // End MainLayout table
  }

  // End the window
  ImGui::End();
}

}  // namespace maia::gui
