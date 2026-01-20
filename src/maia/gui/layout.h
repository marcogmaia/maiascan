// Copyright (c) Maia

#pragma once

#include <imgui.h>

namespace maia::gui {

/// \brief Manages the default window layout for the application.

/// \brief Applies the default docking layout if it hasn't been initialized yet.
/// \param dockspace_id The ID of the main dockspace.
void MakeDefaultLayout(ImGuiID dockspace_id);

}  // namespace maia::gui
