// Copyright (c) Maia

#include "maia/gui/widgets/pointer_scanner_view.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

#include <fmt/core.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "maia/core/pointer_scanner.h"

namespace {

/// \brief Parses a hexadecimal address string to uint64_t.
/// \param hex_str The hexadecimal string to parse.
/// \return The parsed address, or 0 if parsing fails.
[[nodiscard]] uint64_t ParseHexAddress(const std::string& hex_str) {
  if (hex_str.empty()) {
    return 0;
  }
  try {
    return std::stoull(hex_str, nullptr, 16);
  } catch (...) {
    return 0;
  }
}

/// \brief Tokenizes a string by spaces and/or commas, trimming each token.
/// \param input The input string to tokenize.
/// \return Vector of trimmed non-empty tokens.
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

/// \brief Parses a single offset token to an optional int64_t.
/// \details Supports wildcards (* or ?), decimal (default), and hex (0x
/// prefix).
/// \param token The token to parse.
/// \return The parsed offset, or std::nullopt for wildcards.
[[nodiscard]] std::optional<int64_t> ParseOffsetToken(
    const std::string& token) {
  if (token.empty()) {
    return std::nullopt;
  }

  // Check for wildcard
  if (token == "*" || token == "?") {
    return std::nullopt;
  }

  try {
    // Check for hex prefix
    if (token.size() > 2 && token[0] == '0' &&
        (token[1] == 'x' || token[1] == 'X')) {
      return static_cast<int64_t>(std::stoull(token, nullptr, 16));
    }
    // Default to decimal
    return static_cast<int64_t>(std::stoll(token, nullptr, 10));
  } catch (...) {
    return std::nullopt;
  }
}

/// \brief Shows a help marker (?) with tooltip on hover.
/// \param desc The tooltip text to display.
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

}  // namespace

namespace maia {

PointerScannerView::PointerScannerView() = default;

core::PointerScanConfig PointerScannerView::GetScanConfig() const {
  core::PointerScanConfig config;

  config.target_address = ParseHexAddress(target_address_str_);

  config.max_level = static_cast<uint32_t>(max_level_);
  config.max_offset = static_cast<uint32_t>(max_offset_);
  config.allow_negative_offsets = allow_negative_offsets_;
  config.max_results = static_cast<uint32_t>(max_results_);

  // Parse allowed modules from filter input
  config.allowed_modules.clear();
  for (const auto& module : Tokenize(module_filter_input_)) {
    config.allowed_modules.insert(module);
  }

  // Parse last offsets from filter input.
  // User enters offsets in forward order (e.g., "10 58" for [..., +10, +58]).
  // Supports wildcards (* or ?) and decimal (default) or hex (0x prefix).
  // We reverse them so index 0 = last offset (closest to target).
  config.last_offsets.clear();
  for (const auto& offset_str : Tokenize(last_offsets_input_)) {
    config.last_offsets.push_back(ParseOffsetToken(offset_str));
  }
  std::reverse(config.last_offsets.begin(), config.last_offsets.end());

  return config;
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
    PointerScannerView::PathResolver path_resolver) {
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
    RenderResultsSection(paths, is_scanning, path_resolver);
  }
  ImGui::End();
}

void PointerScannerView::RenderTargetSection(
    const std::vector<CheatTableEntry>& cheat_entries,
    const ScanStorage& scan_results) {
  ImGui::Text("Target Address");
  ShowHelpMarker(
      "The memory address you want to find a stable pointer path to.");

  // Address input with validation feedback
  ImGui::PushItemWidth(200);
  if (!target_address_valid_) {
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

  if (!target_address_valid_) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();

  // Type selector
  const char* type_names[] = {
      "Byte", "2 Bytes", "4 Bytes", "8 Bytes", "Float", "Double"};
  ScanValueType types[] = {ScanValueType::kUInt8,
                           ScanValueType::kUInt16,
                           ScanValueType::kUInt32,
                           ScanValueType::kUInt64,
                           ScanValueType::kFloat,
                           ScanValueType::kDouble};

  int current_type_idx = 2;  // Default to 4 Bytes (UInt32)
  for (int i = 0; i < IM_ARRAYSIZE(types); i++) {
    if (types[i] == selected_type_) {
      current_type_idx = i;
      break;
    }
  }

  ImGui::PushItemWidth(100);
  if (ImGui::Combo(
          "Type", &current_type_idx, type_names, IM_ARRAYSIZE(type_names))) {
    selected_type_ = types[current_type_idx];
    signals_.target_type_changed.publish(selected_type_);
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();

  // Source selector dropdown
  // Default to Cheat Table if entries exist and no selection made yet
  if (selected_source_ == 0 && !cheat_entries.empty() &&
      target_address_str_.empty()) {
    selected_source_ = 1;  // From Cheat Table
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

  // Show selection dropdown based on source
  if (selected_source_ == 1 && !cheat_entries.empty()) {
    ImGui::PushItemWidth(300);
    std::string preview = selected_cheat_index_ >= 0
                              ? cheat_entries[selected_cheat_index_].description
                              : "Select entry...";
    if (ImGui::BeginCombo("Cheat Entry", preview.c_str())) {
      for (size_t i = 0; i < cheat_entries.size(); ++i) {
        bool is_selected = (selected_cheat_index_ == static_cast<int>(i));
        std::string label = fmt::format("{} (0x{:X})",
                                        cheat_entries[i].description,
                                        cheat_entries[i].address);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          selected_cheat_index_ = static_cast<int>(i);
          signals_.target_from_cheat_selected.publish(i);
          // Update input field
          target_address_str_ = fmt::format("{:X}", cheat_entries[i].address);
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
  } else if (selected_source_ == 2 && !scan_results.addresses.empty()) {
    ImGui::PushItemWidth(300);
    std::string preview =
        selected_scan_index_ >= 0
            ? fmt::format("0x{:X}",
                          scan_results.addresses[selected_scan_index_])
            : "Select address...";
    if (ImGui::BeginCombo("Scan Result", preview.c_str())) {
      // Show up to 100 scan results to avoid UI lag
      size_t max_display = std::min(scan_results.addresses.size(), size_t(100));
      for (size_t i = 0; i < max_display; ++i) {
        bool is_selected = (selected_scan_index_ == static_cast<int>(i));
        std::string label = fmt::format("0x{:X}", scan_results.addresses[i]);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          selected_scan_index_ = static_cast<int>(i);
          signals_.target_from_scan_selected.publish(i);
          target_address_str_ = fmt::format("{:X}", scan_results.addresses[i]);
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      if (scan_results.addresses.size() > 100) {
        ImGui::TextDisabled("... and {} more",
                            scan_results.addresses.size() - 100);
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
  }
}

void PointerScannerView::RenderMapSection(size_t map_entry_count,
                                          float map_progress,
                                          bool is_generating_map) {
  ImGui::Text("Pointer Map");
  ShowHelpMarker(
      "A snapshot of all pointers in memory. Required before scanning.");

  // Action buttons
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

  // Progress bar
  char buf[32];
  snprintf(buf, sizeof(buf), "%.0f%%", map_progress * 100);
  ImGui::ProgressBar(map_progress, ImVec2(200, 0), buf);

  // Status text
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

  // Max Level
  ImGui::PushItemWidth(80);
  ImGui::InputInt("Max Level", &max_level_, 1, 5);
  max_level_ = std::clamp(max_level_, 1, 15);
  ImGui::PopItemWidth();
  ShowHelpMarker(
      "Max pointer chain depth (e.g., 7 = up to 7 dereferences). "
      "Higher values exponentially increase scan time.");

  ImGui::SameLine();

  // Max Offset
  ImGui::PushItemWidth(100);
  ImGui::InputInt("Max Offset", &max_offset_, 1024, 4096);
  max_offset_ = std::clamp(max_offset_, 64, 65536);
  ImGui::PopItemWidth();
  ShowHelpMarker(
      "Maximum byte distance from each pointer. 2048-4096 is "
      "typically sufficient for most structures.");

  ImGui::SameLine();

  // Allow negative offsets
  ImGui::Checkbox("Allow Negative Offsets", &allow_negative_offsets_);
  ShowHelpMarker("Enable if structures use negative indexing (rare).");

  // Max Results
  ImGui::PushItemWidth(100);
  ImGui::InputInt("Max Results (0=unlimited)", &max_results_, 100, 1000);
  max_results_ = std::clamp(max_results_, 0, 1000000);
  ImGui::PopItemWidth();

  // Module filter with dropdown picker
  ImGui::PushItemWidth(350);
  ImGui::InputText("##ModuleFilter", &module_filter_input_);
  ImGui::PopItemWidth();

  ImGui::SameLine();

  // Dropdown button for module selection
  if (ImGui::Button("...##ModuleDropdown")) {
    ImGui::OpenPopup("ModulePickerPopup");
  }

  ImGui::SameLine();
  ImGui::Text("Allowed Modules");

  // Module picker popup with checkboxes
  if (ImGui::BeginPopup("ModulePickerPopup")) {
    ImGui::Text("Select modules to filter:");
    ImGui::Separator();

    // Parse current filter to check which modules are selected
    auto selected = Tokenize(module_filter_input_);
    std::unordered_set<std::string> selected_set(selected.begin(),
                                                 selected.end());

    for (const auto& mod : available_modules) {
      bool is_checked = selected_set.contains(mod);
      if (ImGui::Checkbox(mod.c_str(), &is_checked)) {
        if (is_checked) {
          // Add module to filter
          if (!module_filter_input_.empty()) {
            module_filter_input_ += " ";
          }
          module_filter_input_ += mod;
        } else {
          // Remove module from filter - rebuild the string
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

  // Last offsets filter
  ImGui::PushItemWidth(400);
  ImGui::InputText("Last Offsets", &last_offsets_input_);
  ImGui::PopItemWidth();
  ImGui::TextDisabled(
      "Example: 16 * 88 (decimal, 0x for hex, * or ? for wildcard)");
}

void PointerScannerView::RenderActionSection(bool is_generating_map,
                                             bool is_scanning,
                                             bool has_paths,
                                             float scan_progress) {
  bool busy = is_generating_map || is_scanning;

  if (busy) {
    ImGui::BeginDisabled();
  }

  // Find Paths button
  if (ImGui::Button("Find Paths", ImVec2(120, 0))) {
    signals_.find_paths_pressed.publish();
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::Text("Search for pointer paths from static addresses to target.");
    ImGui::EndTooltip();
  }

  ImGui::SameLine();

  // Validate button (disabled if no paths)
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

  // Cancel button (only enabled when busy)
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

  // Scan progress bar
  char buf[32];
  snprintf(buf, sizeof(buf), "%.0f%%", scan_progress * 100);
  ImGui::ProgressBar(scan_progress, ImVec2(200, 0), buf);
}

void PointerScannerView::RenderResultsSection(
    const std::vector<core::PointerPath>& paths,
    bool is_scanning,
    PointerScannerView::PathResolver path_resolver) {
  if (paths.empty() && !is_scanning) {
    return;
  }

  ImGui::Text("Results");

  // Status and count
  if (is_scanning) {
    ImGui::Text("Scanning...");
  } else {
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

  // Results table with virtualization using ImGuiListClipper
  ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

  // Add Value column when resolver is provided
  const int column_count = path_resolver ? 3 : 2;

  if (ImGui::BeginTable("PointerPaths", column_count, flags)) {
    ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Path");
    if (path_resolver) {
      ImGui::TableSetupColumn(
          "Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    }
    ImGui::TableHeadersRow();

    size_t max_display =
        show_all_results_ ? paths.size()
                          : std::min(paths.size(), kDefaultMaxDisplayedResults);

    // Use ImGuiListClipper for virtualization - only render visible items
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(max_display));
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        ImGui::TableNextRow();

        // Use unique ID for each row to avoid conflicts
        ImGui::PushID(i);

        // Module column
        ImGui::TableSetColumnIndex(0);
        const auto& path = paths[i];
        std::string module_str =
            path.module_name.empty()
                ? fmt::format("0x{:X}", path.base_address)
                : fmt::format("{}+{:X}", path.module_name, path.module_offset);

        // Selectable spanning all columns
        bool selected = false;
        if (ImGui::Selectable(module_str.c_str(),
                              selected,
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
          if (ImGui::IsMouseDoubleClicked(0)) {
            signals_.result_double_clicked.publish(i);
          }
        }

        // Path column
        ImGui::TableSetColumnIndex(1);
        std::string path_str = FormatPointerPath(path);
        ImGui::Text("%s", path_str.c_str());

        // Value column (if resolver provided)
        if (path_resolver) {
          ImGui::TableSetColumnIndex(2);
          auto resolved = path_resolver(path);
          if (resolved) {
            ImGui::Text("0x%llX", *resolved);
          } else {
            ImGui::TextDisabled("???");
          }
        }

        ImGui::PopID();
      }
    }

    ImGui::EndTable();
  }
}

std::string PointerScannerView::FormatPointerPath(
    const core::PointerPath& path) const {
  // Format: +off1+off2+off3 (Cheat Engine style)
  std::string result;

  for (size_t i = 0; i < path.offsets.size(); ++i) {
    if (path.offsets[i] >= 0) {
      result += fmt::format("+{:X}", path.offsets[i]);
    } else {
      result += fmt::format("-{:X}", -path.offsets[i]);
    }
  }

  return result;
}

}  // namespace maia
