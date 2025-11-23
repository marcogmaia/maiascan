// Copyright (c) Maia

#pragma once

#include <optional>

namespace maia::mmem::utils {

/// \brief Gets the Process ID (PID) of the window directly under
///        the mouse cursor.
/// \return The PID as a std::optional, or std::nullopt if no
///         window or PID was found.
std::optional<uint32_t> GetProcessIdFromCursor();

}  // namespace maia::mmem::utils
