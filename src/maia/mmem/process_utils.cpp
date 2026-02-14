// Copyright (c) Maia

#include "maia/mmem/process_utils.h"

#include <Windows.h>

#include <optional>

namespace maia::mmem::utils {

std::optional<uint32_t> GetProcessIdFromCursor() {
  POINT p;
  if (!::GetCursorPos(&p)) {
    return std::nullopt;
  }

  HWND hwnd_under_cursor = ::WindowFromPoint(p);
  if (!hwnd_under_cursor) {
    return std::nullopt;
  }

  DWORD pid = 0;
  ::GetWindowThreadProcessId(hwnd_under_cursor, &pid);

  if (pid == 0) {
    return std::nullopt;
  }

  return pid;
}

}  // namespace maia::mmem::utils
