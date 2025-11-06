// Copyright (c) Maia

#include "maia/gui/widgets/scanner_view.h"

// TODO: Remove this when changing the array to enum.
#include <array>

namespace maia {

namespace {

std::optional<uint32_t> ToUint32(const std::string& str) {
  const char* first = str.data();
  const char* last = first + str.size();

  uint32_t value;
  std::from_chars_result result = std::from_chars(first, last, value);

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

}  // namespace

void ScannerWidget::Render(const std::vector<ScanEntry>& entries) {
  if (ImGui::Begin("Scanner")) {
    // TODO: Change these to an enum.
    constexpr auto kScanTypeItems =
        std::array{"Exact Value", "Bigger Than...", "Smaller Than..."};
    constexpr auto kValueTypeItems = std::array{
        "4 Bytes", "1 Byte", "2 Bytes", "8 Bytes", "Float", "Double"};

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

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Scan Type");

      ImGui::TableSetColumnIndex(1);
      ImGui::PushItemWidth(-FLT_MIN);
      ImGui::Combo("##ScanType",
                   &current_scan_type_,
                   kScanTypeItems.data(),
                   kScanTypeItems.size());
      ImGui::PopItemWidth();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Value Type");

      ImGui::TableSetColumnIndex(1);
      ImGui::PushItemWidth(-FLT_MIN);
      ImGui::Combo("##ValueType",
                   &current_scan_value_type_,
                   kValueTypeItems.data(),
                   kValueTypeItems.size());
      ImGui::PopItemWidth();

      ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::BeginChild("Table")) {
      auto needle_bytes = ToUint32(str_)
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
          if (entry.data.size() >= sizeof(uint32_t)) {
            auto val = *reinterpret_cast<const uint32_t*>(entry.data.data());
            ImGui::TextUnformatted(std::format("{}", val).c_str());
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
