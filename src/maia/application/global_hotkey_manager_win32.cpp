// Copyright (c) Maia

#include "maia/application/global_hotkey_manager.h"

#include <Windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "maia/logging.h"

namespace maia {

namespace {

// Window property name to store our manager instance pointer
constexpr const wchar_t* kHotkeyManagerProp = L"MaiaHotkeyManager";

// Forward declaration for the window procedure
class Win32GlobalHotkeyManager;
LRESULT CALLBACK HotkeyWindowProc(HWND hwnd,
                                  UINT msg,
                                  WPARAM wparam,
                                  LPARAM lparam);

// Convert our cross-platform modifiers to Win32 modifiers
UINT ToWin32Modifiers(KeyModifier modifiers) {
  UINT result = 0;
  if (static_cast<uint8_t>(modifiers) &
      static_cast<uint8_t>(KeyModifier::kAlt)) {
    result |= MOD_ALT;
  }
  if (static_cast<uint8_t>(modifiers) &
      static_cast<uint8_t>(KeyModifier::kControl)) {
    result |= MOD_CONTROL;
  }
  if (static_cast<uint8_t>(modifiers) &
      static_cast<uint8_t>(KeyModifier::kShift)) {
    result |= MOD_SHIFT;
  }
  if (static_cast<uint8_t>(modifiers) &
      static_cast<uint8_t>(KeyModifier::kSuper)) {
    result |= MOD_WIN;
  }
  return result;
}

// Convert our cross-platform KeyCode to Win32 virtual key code
UINT ToWin32KeyCode(KeyCode key) {
  switch (key) {
    // Letters
    case KeyCode::kA:
      return 'A';
    case KeyCode::kB:
      return 'B';
    case KeyCode::kC:
      return 'C';
    case KeyCode::kD:
      return 'D';
    case KeyCode::kE:
      return 'E';
    case KeyCode::kF:
      return 'F';
    case KeyCode::kG:
      return 'G';
    case KeyCode::kH:
      return 'H';
    case KeyCode::kI:
      return 'I';
    case KeyCode::kJ:
      return 'J';
    case KeyCode::kK:
      return 'K';
    case KeyCode::kL:
      return 'L';
    case KeyCode::kM:
      return 'M';
    case KeyCode::kN:
      return 'N';
    case KeyCode::kO:
      return 'O';
    case KeyCode::kP:
      return 'P';
    case KeyCode::kQ:
      return 'Q';
    case KeyCode::kR:
      return 'R';
    case KeyCode::kS:
      return 'S';
    case KeyCode::kT:
      return 'T';
    case KeyCode::kU:
      return 'U';
    case KeyCode::kV:
      return 'V';
    case KeyCode::kW:
      return 'W';
    case KeyCode::kX:
      return 'X';
    case KeyCode::kY:
      return 'Y';
    case KeyCode::kZ:
      return 'Z';

    // Numbers
    case KeyCode::k0:
      return '0';
    case KeyCode::k1:
      return '1';
    case KeyCode::k2:
      return '2';
    case KeyCode::k3:
      return '3';
    case KeyCode::k4:
      return '4';
    case KeyCode::k5:
      return '5';
    case KeyCode::k6:
      return '6';
    case KeyCode::k7:
      return '7';
    case KeyCode::k8:
      return '8';
    case KeyCode::k9:
      return '9';

    // Function keys
    case KeyCode::kF1:
      return VK_F1;
    case KeyCode::kF2:
      return VK_F2;
    case KeyCode::kF3:
      return VK_F3;
    case KeyCode::kF4:
      return VK_F4;
    case KeyCode::kF5:
      return VK_F5;
    case KeyCode::kF6:
      return VK_F6;
    case KeyCode::kF7:
      return VK_F7;
    case KeyCode::kF8:
      return VK_F8;
    case KeyCode::kF9:
      return VK_F9;
    case KeyCode::kF10:
      return VK_F10;
    case KeyCode::kF11:
      return VK_F11;
    case KeyCode::kF12:
      return VK_F12;

    // Special keys
    case KeyCode::kEscape:
      return VK_ESCAPE;
    case KeyCode::kTab:
      return VK_TAB;
    case KeyCode::kSpace:
      return VK_SPACE;
    case KeyCode::kReturn:
      return VK_RETURN;
    case KeyCode::kBackspace:
      return VK_BACK;
    case KeyCode::kDelete:
      return VK_DELETE;
    case KeyCode::kInsert:
      return VK_INSERT;
    case KeyCode::kHome:
      return VK_HOME;
    case KeyCode::kEnd:
      return VK_END;
    case KeyCode::kPageUp:
      return VK_PRIOR;
    case KeyCode::kPageDown:
      return VK_NEXT;
    case KeyCode::kLeft:
      return VK_LEFT;
    case KeyCode::kUp:
      return VK_UP;
    case KeyCode::kRight:
      return VK_RIGHT;
    case KeyCode::kDown:
      return VK_DOWN;

    // Plus/Minus
    case KeyCode::kPlus:
      return VK_OEM_PLUS;
    case KeyCode::kMinus:
      return VK_OEM_MINUS;

    // Numpad
    case KeyCode::kNumpad0:
      return VK_NUMPAD0;
    case KeyCode::kNumpad1:
      return VK_NUMPAD1;
    case KeyCode::kNumpad2:
      return VK_NUMPAD2;
    case KeyCode::kNumpad3:
      return VK_NUMPAD3;
    case KeyCode::kNumpad4:
      return VK_NUMPAD4;
    case KeyCode::kNumpad5:
      return VK_NUMPAD5;
    case KeyCode::kNumpad6:
      return VK_NUMPAD6;
    case KeyCode::kNumpad7:
      return VK_NUMPAD7;
    case KeyCode::kNumpad8:
      return VK_NUMPAD8;
    case KeyCode::kNumpad9:
      return VK_NUMPAD9;
    case KeyCode::kNumpadAdd:
      return VK_ADD;
    case KeyCode::kNumpadSubtract:
      return VK_SUBTRACT;
    case KeyCode::kNumpadMultiply:
      return VK_MULTIPLY;
    case KeyCode::kNumpadDivide:
      return VK_DIVIDE;
    case KeyCode::kNumpadEnter:
      return VK_RETURN;
    case KeyCode::kNumpadDecimal:
      return VK_DECIMAL;
    case KeyCode::kUnknown:
      return 0;
    default:
      return 0;
  }
}

// Windows implementation using window subclassing
class Win32GlobalHotkeyManager : public GlobalHotkeyManager {
 public:
  explicit Win32GlobalHotkeyManager(void* glfw_window_handle) {
    hwnd_ = glfwGetWin32Window(static_cast<GLFWwindow*>(glfw_window_handle));

    // Store our instance pointer in the window
    SetPropW(hwnd_, kHotkeyManagerProp, reinterpret_cast<HANDLE>(this));

    // Subclass the window procedure
    // NOLINTNEXTLINE
    auto original_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HotkeyWindowProc)));

    // Store the original procedure for later restoration
    SetPropW(
        hwnd_, L"OriginalWndProc", reinterpret_cast<HANDLE>(original_proc));

    LogInfo("Win32GlobalHotkeyManager initialized with window subclassing");
  }

  ~Win32GlobalHotkeyManager() override {
    // Unregister all hotkeys
    for (int id : registered_ids_) {
      UnregisterHotKey(hwnd_, id);
    }

    // Restore original window procedure
    auto original_proc =
        reinterpret_cast<WNDPROC>(GetPropW(hwnd_, L"OriginalWndProc"));
    if (original_proc) {
      SetWindowLongPtrW(
          hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_proc));
    }

    // Remove our properties
    RemovePropW(hwnd_, kHotkeyManagerProp);
    RemovePropW(hwnd_, L"OriginalWndProc");
  }

  void Register(int id, KeyModifier modifiers, KeyCode key) override {
    UINT win_mods = ToWin32Modifiers(modifiers);
    UINT win_key = ToWin32KeyCode(key);

    if (win_key == 0) {
      LogWarning("Cannot register hotkey ID {}: unsupported key code", id);
      return;
    }

    if (::RegisterHotKey(hwnd_, id, win_mods, win_key)) {
      registered_ids_.push_back(id);
      LogInfo("Registered global hotkey ID {} (mods={:04X}, key={:04X})",
              id,
              win_mods,
              win_key);
    } else {
      LogWarning("Failed to register global hotkey ID {}. It might be in use.",
                 id);
    }
  }

  void Unregister(int id) override {
    UnregisterHotKey(hwnd_, id);
  }

  void Poll() override {
    // No-op: We use window subclassing instead of polling
    // This method exists for API compatibility with other platforms
  }

 private:
  HWND hwnd_;
  std::vector<int> registered_ids_;

  // Allow the window proc to access our signal
  friend LRESULT CALLBACK HotkeyWindowProc(HWND, UINT, WPARAM, LPARAM);
};

// Our subclassed window procedure
LRESULT CALLBACK HotkeyWindowProc(HWND hwnd,
                                  UINT msg,
                                  WPARAM wparam,
                                  LPARAM lparam) {
  // Get the original window procedure
  auto original_proc =
      reinterpret_cast<WNDPROC>(GetPropW(hwnd, L"OriginalWndProc"));

  // Get the manager instance
  auto* manager = reinterpret_cast<Win32GlobalHotkeyManager*>(
      GetPropW(hwnd, kHotkeyManagerProp));

  if (msg == WM_HOTKEY && manager) {
    int hotkey_id = static_cast<int>(wparam);
    LogDebug("Global hotkey triggered: ID {}", hotkey_id);
    manager->signal_.publish(hotkey_id);
    // Return 0 to indicate we handled it
    return 0;
  }

  // Forward to original window procedure
  if (original_proc) {
    return CallWindowProcW(original_proc, hwnd, msg, wparam, lparam);
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

// Factory function implementation for Windows
extern std::unique_ptr<GlobalHotkeyManager> CreateWin32HotkeyManager(
    void* glfw_window_handle) {
  return std::make_unique<Win32GlobalHotkeyManager>(glfw_window_handle);
}

}  // namespace maia
