// Copyright (c) Maia

#pragma once

#include <imgui.h>
#include <chrono>
#include <functional>

namespace maia::gui {

/// \brief Linearly interpolates between two ImVec4 colors (RGBA).
/// \param start Starting color.
/// \param end Target color.
/// \param t Interpolation factor (0.0 to 1.0).
/// \return Interpolated color.
[[nodiscard]] ImVec4 LerpColor(const ImVec4& start, const ImVec4& end, float t);

/// \brief Calculates blink alpha for value change highlighting.
/// \param last_change_time Time when value last changed. Zero epoch = never
/// changed.
/// \return Alpha from 1.0 (just changed) to 0.0 over 2 seconds.
[[nodiscard]] float CalculateBlinkAlpha(
    std::chrono::steady_clock::time_point last_change_time);

/// \brief Draws text with a red blink effect when value recently changed.
/// \param last_change_time Time when last changed.
/// \param draw_fn Function to perform the actual drawing (e.g., ImGui::Text).
void DrawWithBlinkEffect(std::chrono::steady_clock::time_point last_change_time,
                         const std::function<void()>& draw_fn);

}  // namespace maia::gui
