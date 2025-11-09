// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

#include <format>
#include <optional>
#include <string_view>

#include <imgui_stdlib.h>

namespace maia {

namespace {

std::optional<uint32_t> ToUint32(std::string_view sview, int base = 10) {
  if (base == 16 && sview.starts_with("0x")) {
    sview = sview.substr(2);
  }
  const char* first = sview.data();
  const char* last = first + sview.size();

  uint32_t value;
  std::from_chars_result result = std::from_chars(first, last, value, base);

  if (result.ec != std::errc() || result.ptr != last) {
    return std::nullopt;
  }
  return value;
}

std::vector<std::byte> ToByteVector(uint32_t value) {
  std::vector<std::byte> bytes(
      reinterpret_cast<std::byte*>(&value),
      reinterpret_cast<std::byte*>(&value) + sizeof(value));
  return bytes;
}

void TextEntryValue(const ScanEntry& entry, bool is_hexadecimal = false) {
  // TODO: Make this safer, and enable the printing of other data types.
  auto value_to_show = *reinterpret_cast<const uint32_t*>(entry.data.data());
  if (is_hexadecimal) {
    ImGui::TextUnformatted(std::format("0x{:x}", value_to_show).c_str());
  } else {
    ImGui::TextUnformatted(std::format("{}", value_to_show).c_str());
  }
}

}  // namespace

void ScannerWidget::Render(const std::vector<ScanEntry>& entries) {
  if (ImGui::Begin("Scanner")) {
    if (ImGui::BeginTable("InputTable", 2)) {
      ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Value:");

      ImGui::TableSetColumnIndex(1);
      // Makes the input fill the cell.
      ImGui::PushItemWidth(-FLT_MIN);
      ImGui::InputText("##Input", &str_);
      ImGui::PopItemWidth();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Hex");

      ImGui::TableSetColumnIndex(1);
      ImGui::Checkbox("##HexInput", &is_hex_input_);

      ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::BeginChild("Table")) {
      const int base = is_hex_input_ ? 16 : 10;
      auto needle_bytes = ToUint32(str_, base)
                              .transform(ToByteVector)
                              .value_or(std::vector<std::byte>());
      if (ImGui::Button("First Scan")) {
        signals_.new_scan_pressed.publish(needle_bytes);
      }
      ImGui::SameLine();
      if (ImGui::Button("Scan")) {
        signals_.scan_button_pressed.publish(needle_bytes);
      }

      ImGui::SameLine();
      if (ImGui::Button("Filter Changed")) {
        signals_.filter_changed.publish();
      }
      ImGui::SetItemTooltip("Filter out all values\nthat have been changed.");

      const ImGuiTableFlags flags = ImGuiTableFlags_RowBg;

      if (ImGui::BeginTable("Tab", 2, flags)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Value");

        ImGui::TableHeadersRow();

        for (int i = 0; i < entries.size(); ++i) {
          const auto& entry = entries[i];  // NOLINT
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          const bool is_selected = (selected_index_ == i);
          std::string address_str = std::format("0x{:x}", entry.address);

          // ImGuiSelectableFlags_SpanAllColumns makes it fill the whole row.
          if (ImGui::Selectable(address_str.c_str(),
                                is_selected,
                                ImGuiSelectableFlags_SpanAllColumns)) {
            selected_index_ = i;
            signals_.entry_selected.publish(entry);
          }

          ImGui::TableNextColumn();
          // TODO: Make this work with other fundamental data types.
          if (entry.data.size() >= sizeof(uint32_t)) {
            TextEntryValue(entry, is_hex_input_);
          } else {
            ImGui::TextUnformatted("N/A");
          }
        }
        ImGui::EndTable();
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

}  // namespace maia
