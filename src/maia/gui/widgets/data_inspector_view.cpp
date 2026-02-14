#include "maia/gui/widgets/data_inspector_view.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include "imgui.h"
#include "maia/gui/models/hex_view_model.h"

namespace maia::gui {

namespace {

template <typename T>
void DrawRow(const char* label, const HexViewModel& model, uintptr_t address) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(label);
  ImGui::TableNextColumn();

  std::array<std::byte, sizeof(T)> buffer;
  if (model.ReadValue(address, sizeof(T), buffer.data())) {
    const T value = std::bit_cast<T>(buffer);

    if constexpr (std::is_floating_point_v<T>) {
      ImGui::Text("%.9g", static_cast<double>(value));
    } else if constexpr (std::is_signed_v<T>) {
      ImGui::Text("%lld", static_cast<long long>(value));
    } else {
      ImGui::Text("%llu", static_cast<unsigned long long>(value));
    }
  } else {
    ImGui::TextDisabled("??");
  }
}

template <typename F>
void DrawGroup(const char* group_name, F&& draw_fn) {
  ImGui::SeparatorText(group_name);
  if (ImGui::BeginTable(group_name,
                        2,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter)) {
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None);
    // ImGui::TableHeadersRow();

    draw_fn();
    ImGui::EndTable();
  }
}

}  // namespace

DataInspectorView::DataInspectorView(HexViewModel& hex_view_model)
    : hex_view_model_(hex_view_model) {}

void DataInspectorView::Render() {
  const auto selection = hex_view_model_.GetSelectionRange();
  if (selection.start == ~0ULL) {
    ImGui::TextDisabled("No memory selected.");
    return;
  }

  const uintptr_t address = selection.start;

  DrawGroup("1 Byte", [&] {
    DrawRow<int8_t>("Int8", hex_view_model_, address);
    DrawRow<uint8_t>("UInt8", hex_view_model_, address);
  });

  DrawGroup("2 Bytes", [&] {
    DrawRow<int16_t>("Int16", hex_view_model_, address);
    DrawRow<uint16_t>("UInt16", hex_view_model_, address);
  });

  DrawGroup("4 Bytes", [&] {
    DrawRow<int32_t>("Int32", hex_view_model_, address);
    DrawRow<uint32_t>("UInt32", hex_view_model_, address);
    DrawRow<float>("Float", hex_view_model_, address);
  });

  DrawGroup("8 Bytes", [&] {
    DrawRow<int64_t>("Int64", hex_view_model_, address);
    DrawRow<uint64_t>("UInt64", hex_view_model_, address);
    DrawRow<double>("Double", hex_view_model_, address);
  });
}

}  // namespace maia::gui
