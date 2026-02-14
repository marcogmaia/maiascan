// Copyright (c) Maia

#include "maia/gui/widgets/pointer_scanner_view.h"

#include <algorithm>
#include <unordered_set>

#include <fmt/core.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "maia/core/pointer_scanner.h"
#include "maia/core/string_utils.h"
#include "maia/core/value_formatter.h"
#include "maia/gui/imgui_effects.h"

namespace maia {

namespace {

[[nodiscard]] std::optional<uint64_t> ParseHexAddress(
    const std::string& hex_str) {
  return core::ParseNumber<uint64_t>(hex_str, 16);
}

[[nodiscard]] std::vector<std::string> Tokenize(const std::string& input) {
  std::vector<std::string> result;
  if (input.empty()) {
    return result;
  }

  std::string current;
  for (char c : input) {
    if (c == ' ' || c == '\t' || c == ',') {
      if (!current.empty()) {
        result.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    result.push_back(current);
  }
  return result;
}

[[nodiscard]] std::optional<int64_t> ParseOffsetToken(
    const std::string& token) {
  if (token.empty()) {
    return std::nullopt;
  }

  if (token == "*" || token == "?") {
    return std::nullopt;
  }

  return core::ParseNumber<int64_t>(token, 0);
}

void ShowHelpMarker(const char* desc) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

std::string FormatPointerPath(const core::PointerPath& path) {
  std::string result;

  for (const auto offset : path.offsets) {
    if (offset >= 0) {
      result += fmt::format("+{:X}", offset);
    } else {
      result += fmt::format("-{:X}", -offset);
    }
  }

  return result;
}

void DrawCheatTableCombo(const std::vector<CheatTableEntry>& entries,
                         int selected_index,
                         std::function<void(int)> on_selected) {
  if (entries.empty()) {
    return;
  }

  ImGui::PushItemWidth(300);
  std::string preview = selected_index >= 0
                            ? entries[selected_index].description
                            : "Select entry...";
  if (ImGui::BeginCombo("Cheat Entry", preview.c_str())) {
    for (int i = 0; i < entries.size(); ++i) {
      bool is_selected = selected_index == i;
      std::string label = fmt::format(
          "{} (0x{:X})", entries[i].description, entries[i].address);
      if (ImGui::Selectable(label.c_str(), is_selected)) {
        on_selected(i);
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();
}

void DrawScanResultCombo(const ScanStorage& results,
                         int selected_index,
                         std::function<void(int)> on_selected) {
  if (results.addresses.empty()) {
    return;
  }

  ImGui::PushItemWidth(300);
  std::string preview =
      selected_index >= 0
          ? fmt::format("0x{:X}", results.addresses[selected_index])
          : "Select address...";
  if (ImGui::BeginCombo("Scan Result", preview.c_str())) {
    int max_display = std::min(static_cast<int>(results.addresses.size()), 100);
    for (int i = 0; i < max_display; ++i) {
      bool is_selected = selected_index == i;
      std::string label = fmt::format("0x{:X}", results.addresses[i]);
      if (ImGui::Selectable(label.c_str(), is_selected)) {
        on_selected(i);
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    if (results.addresses.size() > 100) {
      ImGui::TextDisabled("... and %zu more", results.addresses.size() - 100);
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();
}

using PathResolver = PointerScannerView::PathResolver;
using ValueReader = PointerScannerView::ValueReader;
using RowState = PointerScannerView::RowState;

int CalculateResultsColumnCount(PathResolver resolver, ValueReader reader) {
  int count = 2;
  if (resolver) {
    count = 3;
  }
  if (reader) {
    count = 4;
  }
  return count;
}

void SetupResultsTableColumns(PathResolver path_resolver,
                              ValueReader value_reader) {
  ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthFixed, 200.0f);
  ImGui::TableSetupColumn("Path");
  if (path_resolver) {
    ImGui::TableSetupColumn(
        "Address", ImGuiTableColumnFlags_WidthFixed, 130.0f);
  }
  if (value_reader) {
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
  }
  ImGui::TableHeadersRow();
}

std::string GetModuleCellText(const core::PointerPath& path) {
  if (path.module_name.empty()) {
    return fmt::format("0x{:X}", path.base_address);
  }
  return fmt::format("{}+{:X}", path.module_name, path.module_offset);
}

void DrawModuleCell(const core::PointerPath& path,
                    std::function<void()> on_double_click) {
  std::string module_str = GetModuleCellText(path);
  bool selected = false;
  if (ImGui::Selectable(module_str.c_str(),
                        selected,
                        ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowDoubleClick)) {
    if (ImGui::IsMouseDoubleClicked(0)) {
      on_double_click();
    }
  }
}

void DrawPathCell(const core::PointerPath& path) {
  std::string path_str = FormatPointerPath(path);
  ImGui::TextUnformatted(path_str.c_str());
}

void DrawAddressCell(PathResolver path_resolver,
                     const core::PointerPath& path,
                     std::optional<uint64_t>& out_resolved) {
  out_resolved = path_resolver(path);
  if (out_resolved) {
    ImGui::TextUnformatted(core::FormatAddressHex(*out_resolved).c_str());
  } else {
    ImGui::TextDisabled("???");
  }
}

void UpdateRowState(RowState& state,
                    const std::string& current_value,
                    std::chrono::steady_clock::time_point now) {
  if (state.last_value != current_value) {
    state.last_value = current_value;
    state.last_change = now;
  }
}

void DrawValueCell(ValueReader value_reader,
                   uint64_t resolved_address,
                   ScanValueType value_type,
                   std::unordered_map<std::string, RowState>& row_states,
                   const core::PointerPath& path,
                   std::chrono::steady_clock::time_point now) {
  auto value_data = value_reader(resolved_address);
  if (!value_data || value_data->empty()) {
    ImGui::TextDisabled("???");
    return;
  }

  std::string current_value =
      ValueFormatter::Format(*value_data, value_type, false);
  std::string path_key = core::FormatPointerPathKey(path);
  auto& state = row_states[path_key];

  UpdateRowState(state, current_value, now);

  gui::DrawWithBlinkEffect(state.last_change, [&current_value]() {
    ImGui::TextUnformatted(current_value.c_str());
  });
}

void RenderResultRow(const core::PointerPath& path,
                     int row_index,
                     PathResolver path_resolver,
                     ValueReader value_reader,
                     ScanValueType value_type,
                     std::unordered_map<std::string, RowState>& row_states,
                     std::function<void(int)> on_double_click,
                     std::chrono::steady_clock::time_point now) {
  ImGui::TableNextRow();
  ImGui::PushID(row_index);

  ImGui::TableSetColumnIndex(0);
  DrawModuleCell(
      path, [on_double_click, row_index]() { on_double_click(row_index); });

  ImGui::TableSetColumnIndex(1);
  DrawPathCell(path);

  std::optional<uint64_t> resolved_address;
  if (path_resolver) {
    ImGui::TableSetColumnIndex(2);
    DrawAddressCell(path_resolver, path, resolved_address);
  }

  if (value_reader) {
    ImGui::TableSetColumnIndex(path_resolver ? 3 : 2);
    if (resolved_address) {
      DrawValueCell(
          value_reader, *resolved_address, value_type, row_states, path, now);
    } else {
      ImGui::TextDisabled("-");
    }
  }

  ImGui::PopID();
}

void RenderResultsTable(const std::vector<core::PointerPath>& paths,
                        bool show_all_results,
                        PathResolver path_resolver,
                        ValueReader value_reader,
                        ScanValueType value_type,
                        std::unordered_map<std::string, RowState>& row_states,
                        std::function<void(int)> on_double_click) {
  int column_count = CalculateResultsColumnCount(path_resolver, value_reader);
  ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

  if (!ImGui::BeginTable("PointerPaths", column_count, flags)) {
    return;
  }

  SetupResultsTableColumns(path_resolver, value_reader);

  size_t max_display =
      show_all_results
          ? paths.size()
          : std::min(paths.size(),
                     PointerScannerView::kDefaultMaxDisplayedResults);

  auto now = std::chrono::steady_clock::now();
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(max_display));

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
      RenderResultRow(paths[i],
                      i,
                      path_resolver,
                      value_reader,
                      value_type,
                      row_states,
                      on_double_click,
                      now);
    }
  }

  ImGui::EndTable();
}

}  // namespace

PointerScannerView::PointerScannerView() = default;

core::PointerScanConfig PointerScannerView::GetScanConfig() const {
  std::unordered_set<std::string> modules;
  for (const auto& module : Tokenize(module_filter_input_)) {
    modules.insert(module);
  }

  std::vector<std::optional<int64_t>> offsets;
  for (const auto& offset_str : Tokenize(last_offsets_input_)) {
    offsets.push_back(ParseOffsetToken(offset_str));
  }
  std::reverse(offsets.begin(), offsets.end());

  return core::PointerScanConfig{
      .target_address = ParseHexAddress(target_address_str_).value_or(0),
      .max_level = static_cast<uint32_t>(max_level_),
      .max_offset = static_cast<uint32_t>(max_offset_),
      .allow_negative_offsets = allow_negative_offsets_,
      .max_results = static_cast<uint32_t>(max_results_),
      .allowed_modules = std::move(modules),
      .last_offsets = std::move(offsets),
  };
}

void PointerScannerView::SetTargetAddress(uint64_t address) {
  target_address_str_ = core::FormatAddressHex(address);
  signals_.target_address_changed.publish(address);
  target_address_valid_ = true;
}

void PointerScannerView::Render(
    bool* is_open,
    const std::vector<core::PointerPath>& paths,
    size_t map_entry_count,
    float map_progress,
    float scan_progress,
    bool is_generating_map,
    bool is_scanning,
    const std::vector<CheatTableEntry>& cheat_entries,
    const ScanStorage& scan_results,
    const std::vector<std::string>& available_modules,
    PointerScannerView::PathResolver path_resolver,
    PointerScannerView::ValueReader value_reader,
    ScanValueType value_type) {
  if (!is_open || !*is_open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Pointer Scanner", is_open)) {
    RenderTargetSection(cheat_entries, scan_results);
    ImGui::Separator();
    RenderMapSection(map_entry_count, map_progress, is_generating_map);
    ImGui::Separator();
    RenderConfigSection(available_modules);
    ImGui::Separator();
    RenderActionSection(
        is_generating_map, is_scanning, !paths.empty(), scan_progress);
    ImGui::Separator();
    RenderResultsSection(
        paths, is_scanning, path_resolver, value_reader, value_type);
  }
  ImGui::End();
}

void PointerScannerView::RenderTargetSection(
    const std::vector<CheatTableEntry>& cheat_entries,
    const ScanStorage& scan_results) {
  ImGui::Text("Target Address");
  ShowHelpMarker(
      "The memory address you want to find a stable pointer path to.");

  RenderTargetAddressInput();
  ImGui::SameLine();
  RenderTypeSelector();
  ImGui::SameLine();
  RenderSourceSelector(cheat_entries, scan_results);
}

void PointerScannerView::RenderTargetAddressInput() {
  ImGui::PushItemWidth(200);

  bool should_style = !target_address_valid_;
  if (should_style) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 0, 0, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
  }

  if (ImGui::InputText("Hex Address",
                       &target_address_str_,
                       ImGuiInputTextFlags_CharsHexadecimal |
                           ImGuiInputTextFlags_CallbackAlways)) {
    try {
      uint64_t addr = std::stoull(target_address_str_, nullptr, 16);
      target_address_valid_ = true;
      signals_.target_address_changed.publish(addr);
    } catch (...) {
      target_address_valid_ = false;
      signals_.target_address_invalid.publish();
    }
  }

  if (should_style) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }
  ImGui::PopItemWidth();
}

void PointerScannerView::RenderTypeSelector() {
  auto type_names =
      std::array{"Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double"};
  auto types = std::array{ScanValueType::kUInt8,
                          ScanValueType::kUInt16,
                          ScanValueType::kUInt32,
                          ScanValueType::kUInt64,
                          ScanValueType::kFloat,
                          ScanValueType::kDouble};

  const auto to_index = [](ScanValueType t) { return static_cast<int>(t); };

  int current_type_idx = to_index(ScanValueType::kUInt32);
  for (int i = 0; i < types.size(); ++i) {
    if (types[i] == selected_type_) {
      current_type_idx = i;
      break;
    }
  }

  ImGui::PushItemWidth(100);
  if (ImGui::Combo(
          "Type", &current_type_idx, type_names.data(), type_names.size())) {
    selected_type_ = types[current_type_idx];
    signals_.target_type_changed.publish(selected_type_);
  }
  ImGui::PopItemWidth();
}

void PointerScannerView::RenderSourceSelector(
    const std::vector<CheatTableEntry>& cheat_entries,
    const ScanStorage& scan_results) {
  if (selected_source_ == 0 && !cheat_entries.empty() &&
      target_address_str_.empty()) {
    selected_source_ = 1;
  }

  const char* sources[] = {
      "Manual Entry", "From Cheat Table", "From Scan Results"};
  ImGui::PushItemWidth(150);
  if (ImGui::Combo(
          "Source", &selected_source_, sources, IM_ARRAYSIZE(sources))) {
    if (selected_source_ == 1 && selected_cheat_index_ >= 0) {
      signals_.target_from_cheat_selected.publish(
          static_cast<size_t>(selected_cheat_index_));
    } else if (selected_source_ == 2 && selected_scan_index_ >= 0) {
      signals_.target_from_scan_selected.publish(
          static_cast<size_t>(selected_scan_index_));
    }
  }
  ImGui::PopItemWidth();

  if (selected_source_ == 1) {
    DrawCheatTableCombo(
        cheat_entries, selected_cheat_index_, [this, &cheat_entries](int i) {
          selected_cheat_index_ = i;
          signals_.target_from_cheat_selected.publish(i);
          if (!cheat_entries.at(i).IsDynamicAddress()) {
            SetTargetAddress(cheat_entries.at(i).address);
          }
        });
  } else if (selected_source_ == 2) {
    DrawScanResultCombo(
        scan_results, selected_scan_index_, [this, &scan_results](int i) {
          selected_scan_index_ = i;
          signals_.target_from_scan_selected.publish(i);
          target_address_str_ =
              core::FormatAddressHex(scan_results.addresses[i]);
        });
  }
}

void PointerScannerView::RenderMapSection(size_t map_entry_count,
                                          float map_progress,
                                          bool is_generating_map) const {
  ImGui::Text("Pointer Map");
  ShowHelpMarker(
      "A snapshot of all pointers in memory. Required before scanning.");

  if (is_generating_map) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Generate", ImVec2(100, 0))) {
    signals_.generate_map_pressed.publish();
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::Text("Create a new pointer map from current process memory.");
    ImGui::EndTooltip();
  }

  ImGui::SameLine();

  if (ImGui::Button("Save...", ImVec2(100, 0))) {
    signals_.save_map_pressed.publish();
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::Text("Save pointer map to disk for later use.");
    ImGui::EndTooltip();
  }

  ImGui::SameLine();

  if (ImGui::Button("Load...", ImVec2(100, 0))) {
    signals_.load_map_pressed.publish();
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::Text("Load a previously saved pointer map.");
    ImGui::EndTooltip();
  }

  if (is_generating_map) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();

  if (is_generating_map) {
    std::string buf = fmt::format("{:.0f}%", map_progress * 100);
    ImGui::ProgressBar(map_progress, ImVec2(200, 0), buf.c_str());
  }

  if (map_entry_count > 0) {
    ImGui::Text("Status: %zu pointers mapped", map_entry_count);
  } else if (is_generating_map) {
    ImGui::Text("Status: Generating...");
  } else {
    ImGui::Text("Status: No map generated");
  }
}

void PointerScannerView::RenderConfigSection(
    const std::vector<std::string>& available_modules) {
  ImGui::Text("Configuration");

  ImGui::PushItemWidth(80);
  ImGui::InputInt("Max Level", &max_level_, 1, 5);
  max_level_ = std::clamp(max_level_, 1, 15);
  ImGui::PopItemWidth();
  ShowHelpMarker(
      "Max pointer chain depth (e.g., 7 = up to 7 dereferences). "
      "Higher values exponentially increase scan time.");

  ImGui::SameLine();

  ImGui::PushItemWidth(100);
  ImGui::InputInt("Max Offset", &max_offset_, 1024, 4096);
  max_offset_ = std::clamp(max_offset_, 64, 65536);
  ImGui::PopItemWidth();
  ShowHelpMarker(
      "Maximum byte distance from each pointer. 2048-4096 is "
      "typically sufficient for most structures.");

  ImGui::SameLine();

  ImGui::Checkbox("Allow Negative Offsets", &allow_negative_offsets_);
  ShowHelpMarker("Enable if structures use negative indexing (rare).");

  ImGui::PushItemWidth(100);
  ImGui::InputInt("Max Results (0=unlimited)", &max_results_, 100, 1000);
  max_results_ = std::clamp(max_results_, 0, 1000000);
  ImGui::PopItemWidth();

  ImGui::PushItemWidth(350);
  ImGui::InputText("##ModuleFilter", &module_filter_input_);
  ImGui::PopItemWidth();

  ImGui::SameLine();

  if (ImGui::Button("...##ModuleDropdown")) {
    ImGui::OpenPopup("ModulePickerPopup");
  }

  ImGui::SameLine();
  ImGui::Text("Allowed Modules");

  if (ImGui::BeginPopup("ModulePickerPopup")) {
    ImGui::Text("Select modules to filter:");
    ImGui::Separator();

    auto selected = Tokenize(module_filter_input_);
    std::unordered_set<std::string> selected_set(selected.begin(),
                                                 selected.end());

    for (const auto& mod : available_modules) {
      bool is_checked = selected_set.contains(mod);
      if (ImGui::Checkbox(mod.c_str(), &is_checked)) {
        if (is_checked) {
          if (!module_filter_input_.empty()) {
            module_filter_input_ += " ";
          }
          module_filter_input_ += mod;
        } else {
          std::string new_filter;
          for (const auto& s : selected) {
            if (s != mod) {
              if (!new_filter.empty()) {
                new_filter += " ";
              }
              new_filter += s;
            }
          }
          module_filter_input_ = new_filter;
        }
      }
    }

    if (available_modules.empty()) {
      ImGui::TextDisabled("No modules available (generate map first)");
    }

    ImGui::EndPopup();
  }

  ImGui::PushItemWidth(400);
  ImGui::InputText("Last Offsets", &last_offsets_input_);
  ImGui::PopItemWidth();
  ImGui::TextDisabled(
      "Example: 16 * 88 (decimal, 0x for hex, * or ? for wildcard)");
}

void PointerScannerView::RenderActionSection(bool is_generating_map,
                                             bool is_scanning,
                                             bool has_paths,
                                             float scan_progress) const {
  bool busy = is_generating_map || is_scanning;

  if (busy) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Find Paths", ImVec2(120, 0))) {
    signals_.find_paths_pressed.publish();
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::Text("Search for pointer paths from static addresses to target.");
    ImGui::EndTooltip();
  }

  ImGui::SameLine();

  if (!has_paths) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Validate", ImVec2(120, 0))) {
    signals_.validate_pressed.publish();
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::Text("Check which paths still point to the target address.");
    ImGui::EndTooltip();
  }
  if (!has_paths) {
    ImGui::EndDisabled();
  }

  if (busy) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();

  if (!busy) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Cancel", ImVec2(120, 0))) {
    signals_.cancel_pressed.publish();
  }
  if (!busy) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();

  if (is_scanning) {
    std::string buf = fmt::format("{:.0f}%", scan_progress * 100);
    ImGui::ProgressBar(scan_progress, ImVec2(200, 0), buf.c_str());
  }
}

void PointerScannerView::RenderResultsSection(
    const std::vector<core::PointerPath>& paths,
    bool is_scanning,
    PointerScannerView::PathResolver path_resolver,
    PointerScannerView::ValueReader value_reader,
    ScanValueType value_type) {
  if (paths.empty() && !is_scanning) {
    return;
  }

  RenderResultsStatus(paths, is_scanning);

  if (static_cast<const void*>(&paths) != last_paths_ptr_) {
    visible_row_states_.clear();
    last_paths_ptr_ = static_cast<const void*>(&paths);
  }

  RenderResultsTable(
      paths,
      show_all_results_,
      path_resolver,
      value_reader,
      value_type,
      visible_row_states_,
      [this](int i) { signals_.result_double_clicked.publish(i); });
}

void PointerScannerView::RenderResultsStatus(
    const std::vector<core::PointerPath>& paths, bool is_scanning) {
  ImGui::Text("Results");

  if (is_scanning) {
    ImGui::Text("Scanning...");
    return;
  }

  size_t display_count =
      show_all_results_ ? paths.size()
                        : std::min(paths.size(), kDefaultMaxDisplayedResults);

  if (paths.size() > kDefaultMaxDisplayedResults && !show_all_results_) {
    ImGui::Text("Showing %zu of %zu results", display_count, paths.size());
    ImGui::SameLine();
    if (ImGui::Button("Show All")) {
      show_all_results_ = true;
      signals_.show_all_pressed.publish();
    }
  } else {
    ImGui::Text("%zu paths found", paths.size());
  }
}

}  // namespace maia
