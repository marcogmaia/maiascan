#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "maia/core/i_process.h"

namespace maia::gui {

class HexViewModel {
 public:
  void SetProcess(IProcess* process);
  IProcess* GetProcess() const;

  void GoTo(uintptr_t address);
  uintptr_t GetCurrentAddress() const;

  void Scroll(int32_t lines);

  struct SelectionRange {
    uintptr_t start;
    uintptr_t end;
  };

  SelectionRange GetSelectionRange() const;
  void SetSelectionRange(uintptr_t start, uintptr_t end);

  void Refresh();
  void CachePage();  // Keep for backward compatibility or internal use
  const std::vector<std::byte>& GetCachedData() const;
  const std::vector<uint8_t>& GetValidityMask() const;
  const std::unordered_map<uintptr_t, std::byte>& GetEditBuffer() const;

  using Timestamp = std::chrono::steady_clock::time_point;
  const std::unordered_map<uintptr_t, Timestamp>& GetDiffMap() const;

  bool ReadValue(uintptr_t address, size_t size, std::byte* out_buffer) const;

  void SetByte(uintptr_t address, std::byte value);
  void Commit();

 private:
  IProcess* process_ = nullptr;
  uintptr_t current_address_ = 0;
  SelectionRange selection_range_ = {.start = ~0ULL, .end = ~0ULL};
  std::unordered_map<uintptr_t, std::byte> edit_buffer_;
  std::vector<std::byte> cached_data_;
  std::vector<uint8_t> validity_mask_;
  std::vector<maia::MemoryAddress> address_buffer_;
  uintptr_t cached_address_ = 0;

  std::unordered_map<uintptr_t, Timestamp> diff_map_;
};

}  // namespace maia::gui
