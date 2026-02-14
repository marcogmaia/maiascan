// Copyright (c) Maia

#pragma once

#include <entt/signal/sigh.hpp>
#include <memory>

namespace maia {

// Platform-agnostic modifier flags
enum class KeyModifier : uint8_t {
  kNone = 0,
  kControl = 1 << 0,
  kShift = 1 << 1,
  kAlt = 1 << 2,
  kSuper = 1 << 3,  // Windows key / Command key
};

inline KeyModifier operator|(KeyModifier a, KeyModifier b) {
  return static_cast<KeyModifier>(static_cast<uint8_t>(a) |
                                  static_cast<uint8_t>(b));
}

// Common key codes (subset of typical keys used for hotkeys)
enum class KeyCode : uint32_t {
  kUnknown = 0,
  kA = 'A',
  kB = 'B',
  kC = 'C',
  kD = 'D',
  kE = 'E',
  kF = 'F',
  kG = 'G',
  kH = 'H',
  kI = 'I',
  kJ = 'J',
  kK = 'K',
  kL = 'L',
  kM = 'M',
  kN = 'N',
  kO = 'O',
  kP = 'P',
  kQ = 'Q',
  kR = 'R',
  kS = 'S',
  kT = 'T',
  kU = 'U',
  kV = 'V',
  kW = 'W',
  kX = 'X',
  kY = 'Y',
  kZ = 'Z',
  k0 = '0',  // NOLINT
  k1 = '1',  // NOLINT
  k2 = '2',
  k3 = '3',
  k4 = '4',
  k5 = '5',
  k6 = '6',
  k7 = '7',
  k8 = '8',
  k9 = '9',
  kF1 = 0x100,
  kF2 = 257,
  kF3 = 258,
  kF4 = 259,
  kF5 = 260,
  kF6 = 261,
  kF7 = 262,
  kF8 = 263,
  kF9 = 264,
  kF10 = 265,
  kF11 = 266,
  kF12 = 267,
  kEscape = 268,
  kTab = 269,
  kSpace = 270,
  kReturn = 271,
  kBackspace = 272,
  kDelete = 273,
  kInsert = 274,
  kHome = 275,
  kEnd = 276,
  kPageUp = 277,
  kPageDown = 278,
  kLeft = 279,
  kUp = 280,
  kRight = 281,
  kDown = 282,
  kPlus = 283,   // + key (main keyboard)
  kMinus = 284,  // - key (main keyboard)
  kNumpad0 = 285,
  kNumpad1 = 286,
  kNumpad2 = 287,
  kNumpad3 = 288,
  kNumpad4 = 289,
  kNumpad5 = 290,
  kNumpad6 = 291,
  kNumpad7 = 292,
  kNumpad8 = 293,
  kNumpad9 = 294,
  kNumpadAdd = 295,       // + on numpad
  kNumpadSubtract = 296,  // - on numpad
  kNumpadMultiply = 297,
  kNumpadDivide = 298,
  kNumpadEnter = 299,
  kNumpadDecimal = 300,
};

// Abstract interface for global hotkey management
class GlobalHotkeyManager {
 public:
  virtual ~GlobalHotkeyManager() = default;

  // Registers a global hotkey.
  // id: Unique identifier for the hotkey (used when triggered).
  // modifiers: Modifier keys (Control, Shift, Alt, etc.).
  // key: The main key to trigger the hotkey.
  virtual void Register(int id, KeyModifier modifiers, KeyCode key) = 0;

  // Unregisters a previously registered hotkey.
  virtual void Unregister(int id) = 0;

  // Polls for hotkey events. Must be called regularly (e.g., in main loop).
  virtual void Poll() = 0;

  // Sink wrapper for the hotkey signal (follows EnTT pattern)
  struct Sinks {
    GlobalHotkeyManager& manager;

    auto HotkeyTriggered() {
      return entt::sink(manager.signal_);
    }
  };

  auto sinks() {
    return Sinks{*this};
  }

  // Factory function to create platform-specific implementation
  static std::unique_ptr<GlobalHotkeyManager> Create(void* glfw_window_handle);

 protected:
  entt::sigh<void(int)> signal_;
};

}  // namespace maia
