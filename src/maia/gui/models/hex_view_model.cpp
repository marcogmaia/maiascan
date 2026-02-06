#include "maia/gui/models/hex_view_model.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "maia/core/i_process.h"

namespace maia::gui {

void HexViewModel::SetProcess(IProcess* process) {
  process_ = process;
  if (process_) {
    GoTo(process_->GetBaseAddress());
    Refresh();
  }
}

IProcess* HexViewModel::GetProcess() const {
  return process_;
}

void HexViewModel::GoTo(uintptr_t address) {
  if (current_address_ != address) {
    current_address_ = address;
    diff_map_.clear();
  }
}

uintptr_t HexViewModel::GetCurrentAddress() const {
  return current_address_;
}

void HexViewModel::Scroll(int32_t lines) {
  intptr_t offset = static_cast<intptr_t>(lines) * 16;
  uintptr_t new_address = current_address_;
  if (offset < 0) {
    auto abs_offset = static_cast<uintptr_t>(-offset);
    new_address =
        (abs_offset > current_address_) ? 0 : current_address_ - abs_offset;
  } else if (offset > 0) {
    auto u_offset = static_cast<uintptr_t>(offset);
    new_address = (UINTPTR_MAX - current_address_ < u_offset)
                      ? UINTPTR_MAX
                      : current_address_ + u_offset;
  }

  if (new_address != current_address_) {
    current_address_ = new_address;
    diff_map_.clear();
  }
}

HexViewModel::SelectionRange HexViewModel::GetSelectionRange() const {
  return selection_range_;
}

void HexViewModel::SetSelectionRange(uintptr_t start, uintptr_t end) {
  selection_range_ = {.start = start, .end = end};
}

void HexViewModel::Refresh() {
  if (!process_) {
    return;
  }

  // Prune diffs older than 2.0s
  auto now = std::chrono::steady_clock::now();
  for (auto it = diff_map_.begin(); it != diff_map_.end();) {
    if (now - it->second > std::chrono::milliseconds(2000)) {
      it = diff_map_.erase(it);
    } else {
      ++it;
    }
  }

  bool can_diff =
      (current_address_ == cached_address_) && !cached_data_.empty();
  std::vector<std::byte> old_data;
  if (can_diff) {
    old_data = cached_data_;
  }

  const size_t k_page_size = 0x1000;
  cached_data_.resize(k_page_size);
  validity_mask_.resize(k_page_size);
  address_buffer_.resize(k_page_size);
  for (size_t i = 0; i < k_page_size; ++i) {
    address_buffer_[i] = current_address_ + i;
  }
  process_->ReadMemory(address_buffer_, 1, cached_data_, &validity_mask_);

  if (can_diff && cached_data_.size() == old_data.size()) {
    for (size_t i = 0; i < cached_data_.size(); ++i) {
      if (cached_data_[i] != old_data[i] && validity_mask_[i]) {
        diff_map_[current_address_ + i] = now;
      }
    }
  }

  cached_address_ = current_address_;
}

void HexViewModel::CachePage() {
  Refresh();
}

const std::vector<std::byte>& HexViewModel::GetCachedData() const {
  return cached_data_;
}

const std::vector<uint8_t>& HexViewModel::GetValidityMask() const {
  return validity_mask_;
}

const std::unordered_map<uintptr_t, std::byte>& HexViewModel::GetEditBuffer()
    const {
  return edit_buffer_;
}

const std::unordered_map<uintptr_t, HexViewModel::Timestamp>&
HexViewModel::GetDiffMap() const {
  return diff_map_;
}

bool HexViewModel::ReadValue(uintptr_t address,
                             size_t size,
                             std::byte* out_buffer) const {
  if (address < cached_address_ ||
      address + size > cached_address_ + cached_data_.size()) {
    return false;
  }

  size_t offset = address - cached_address_;
  for (size_t i = 0; i < size; ++i) {
    if (!validity_mask_[offset + i]) {
      return false;
    }
  }

  std::memcpy(out_buffer, cached_data_.data() + offset, size);
  return true;
}

void HexViewModel::SetByte(uintptr_t address, std::byte value) {
  edit_buffer_[address] = value;
}

void HexViewModel::Commit() {
  if (!process_) {
    return;
  }

  for (const auto& [addr, val] : edit_buffer_) {
    process_->WriteMemory(addr, {&val, 1});
  }
  edit_buffer_.clear();
}

}  // namespace maia::gui
