// Copyright (c) Maia

#pragma once

#include <memory>
#include <ranges>
#include <vector>

#include "maia/assert2.h"
#include "maia/core/i_process.h"
#include "maia/core/memory_common.h"
#include "maia/core/scan_types.h"

namespace maia {

// using SnapshotPtr = std::shared_ptr<const MemorySnapshot>;

class ScanResult {
 public:
  // Empty result
  ScanResult() = default;

  size_t size() const {
    return snapshot_ ? snapshot_->addresses.size() : 0;
  }

  bool empty() const {
    return size() == 0;
  }

  std::span<const uintptr_t> addresses() const {
    return snapshot_->addresses;
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
        byte_size_(byte_size) {}

  std::shared_ptr<const MemorySnapshot> snapshot_;
  size_t byte_size_ = 0;  // sizeof(T) for the current view

  template <CScannableType T>
  void ValidateSize() const {
    maia::Assert(byte_size_ == sizeof(T));
  }
};

// Read current memory as any type (no snapshot involved).
template <CScannableType T>
T ReadCurrent(IProcess& process, MemoryAddress address) {
  T value;
  process.ReadMemory(address, ToBytesView(value));
  return value;
}

// Write a value (type-safe).
template <CScannableType T>
bool Write(IProcess& process, MemoryAddress address, const T& value) {
  return process.WriteMemory(address, ToBytesView(value));
}

}  // namespace maia
