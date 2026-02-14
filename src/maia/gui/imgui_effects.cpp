// Copyright (c) Maia

#include "maia/gui/imgui_effects.h"

#include <cmath>

namespace maia::gui {

ImVec4 LerpColor(const ImVec4& start, const ImVec4& end, float t) {
  return ImVec4(std::lerp(start.x, end.x, t),
                std::lerp(start.y, end.y, t),
                std::lerp(start.z, end.z, t),
                std::lerp(start.w, end.w, t));
}

float CalculateBlinkAlpha(
    std::chrono::steady_clock::time_point last_change_time) {
  if (last_change_time.time_since_epoch().count() <= 0) {
    return 0.0f;
  }

  constexpr float kBlinkDuration = 2.0f;
  auto now = std::chrono::steady_clock::now();
  float time_since_change =
      std::chrono::duration<float>(now - last_change_time).count();

  if (time_since_change < kBlinkDuration) {
    return 1.0f - (time_since_change / kBlinkDuration);
  }
  return 0.0f;
}

void DrawWithBlinkEffect(std::chrono::steady_clock::time_point last_change_time,
                         const std::function<void()>& draw_fn) {
  const float blink_alpha = CalculateBlinkAlpha(last_change_time);
  const bool is_blinking = blink_alpha > 0.0f;
  if (is_blinking) {
    const auto default_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const auto color_red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    const auto blink_color = LerpColor(default_color, color_red, blink_alpha);
    ImGui::PushStyleColor(ImGuiCol_Text, blink_color);
  }

  draw_fn();

  if (is_blinking) {
    ImGui::PopStyleColor();
  }
}

}  // namespace maia::gui
