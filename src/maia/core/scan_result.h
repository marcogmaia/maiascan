// Copyright (c) Maia

#pragma once

#include <cstring>
#include <memory>
#include <ranges>
#include <vector>

#include "maia/assert.h"
#include "maia/core/memory_common.h"
#include "maia/core/scan_types.h"

namespace maia {

class ScanResult {
 public:
  // Empty result.
  ScanResult() = default;

  size_t size() const {
    return snapshot_ ? snapshot_->addresses.size() : 0;
  }

  bool empty() const {
    return size() == 0;
  }

  std::span<const uintptr_t> addresses() const {
    return snapshot_ ? std::span<const uintptr_t>(snapshot_->addresses)
                     : std::span<const uintptr_t>{};
  }

  // Safe, lazy range of previous values
  template <CScannableType T>
  auto values() const {
    ValidateSize<T>();
    return std::views::iota(0uz, size()) |
           std::views::transform([this](size_t i) -> T {
             T value;
             std::memcpy(&value, &snapshot_->values[i * byte_size_], sizeof(T));
             return value;
           });
  }

  // Reinterpret entire result as different type (zero cost).
  template <CScannableType T>
  ScanResult As() const {
    ScanResult r;
    r.snapshot_ = snapshot_;
    r.byte_size_ = sizeof(T);
    return r;
  }

  template <CScannableType T>
  static ScanResult FromSnapshot(
      std::shared_ptr<const MemorySnapshot> snapshot) {
    return ScanResult(snapshot, sizeof(T));
  }

 private:
  explicit ScanResult(std::shared_ptr<const MemorySnapshot> snapshot,
                      size_t byte_size)
      : snapshot_(std::move(snapshot)),
        byte_size_(byte_size) {
    maia::Assert(!snapshot_ || snapshot_->values.size() ==
                                   snapshot_->addresses.size() * byte_size_);
  }

  std::shared_ptr<const MemorySnapshot> snapshot_;
  size_t byte_size_ = 0;  // sizeof(T) for the current view

  template <CScannableType T>
  void ValidateSize() const {
    maia::Assert(byte_size_ == sizeof(T));
  }
};

}  // namespace maia
