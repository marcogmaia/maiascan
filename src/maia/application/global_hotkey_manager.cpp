// Copyright (c) Maia

#include "maia/application/global_hotkey_manager.h"

namespace maia {

#ifdef _WIN32
// Forward declaration for Windows factory function
extern std::unique_ptr<GlobalHotkeyManager> CreateWin32HotkeyManager(
    void* glfw_window_handle);
#endif  // _WIN32

// Factory implementation - returns platform-specific instance
std::unique_ptr<GlobalHotkeyManager> GlobalHotkeyManager::Create(
    void* glfw_window_handle) {
#ifdef _WIN32
  return CreateWin32HotkeyManager(glfw_window_handle);
#elifdef __APPLE__
  // macOS implementation would go here
  // extern std::unique_ptr<GlobalHotkeyManager> CreateMacOSHotkeyManager(...);
  // return CreateMacOSHotkeyManager(glfw_window_handle);
  (void)glfw_window_handle;  // Suppress unused warning
  return nullptr;            // Not yet implemented
#elif defined(__linux__)
  // Linux implementation would go here
  // extern std::unique_ptr<GlobalHotkeyManager> CreateLinuxHotkeyManager(...);
  // return CreateLinuxHotkeyManager(glfw_window_handle);
  (void)glfw_window_handle;  // Suppress unused warning
  return nullptr;            // Not yet implemented
#else
  (void)glfw_window_handle;  // Suppress unused warning
  return nullptr;            // Unsupported platform
#endif
}

}  // namespace maia
