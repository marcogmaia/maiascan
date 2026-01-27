// Copyright (c) Maia

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <utility>

#include "maia/core/cpu_info.h"

namespace maia::core {

namespace internal {

// Standard library search fallback for scalar execution.
// When alignment > 1, only reports matches at aligned offsets.
template <typename Callback>
void ScanBufferScalar(std::span<const std::byte> buffer,
                      std::span<const std::byte> pattern,
                      size_t alignment,
                      Callback&& callback) {
  if (pattern.empty() || buffer.size() < pattern.size()) {
    return;
  }

  const size_t pattern_size = pattern.size();
  const size_t limit = buffer.size() - pattern_size;

  // Iterate by alignment stride for efficiency.
  for (size_t offset = 0; offset <= limit; offset += alignment) {
    if (std::memcmp(buffer.data() + offset, pattern.data(), pattern_size) ==
        0) {
      callback(offset);
    }
  }
}

template <typename Callback>
void ScanMemCmpScalar(std::span<const std::byte> buf1,
                      std::span<const std::byte> buf2,
                      bool find_equal,
                      size_t stride,
                      Callback&& callback) {
  const size_t size = std::min(buf1.size(), buf2.size());
  const size_t limit = size - (size % stride);

  for (size_t i = 0; i < limit; i += stride) {
    bool equal = (std::memcmp(&buf1[i], &buf2[i], stride) == 0);
    if (equal == find_equal) {
      callback(i);
    }
  }
}

template <typename T, typename Callback>
void ScanMemCompareGreaterScalar(std::span<const std::byte> buf1,
                                 std::span<const std::byte> buf2,
                                 Callback&& callback) {
  const size_t count = std::min(buf1.size(), buf2.size()) / sizeof(T);
  const T* val1 = reinterpret_cast<const T*>(buf1.data());
  const T* val2 = reinterpret_cast<const T*>(buf2.data());

  for (size_t i = 0; i < count; ++i) {
    if (val1[i] > val2[i]) {
      callback(i * sizeof(T));
    }
  }
}

// Non-template AVX2 implementations in simd_scanner.cpp
void ScanBufferAvx2_Impl(std::span<const std::byte> buffer,
                         std::span<const std::byte> pattern,
                         size_t alignment,
                         std::function<void(size_t)> callback);

void ScanMemCmpAvx2_Impl(std::span<const std::byte> buf1,
                         std::span<const std::byte> buf2,
                         bool find_equal,
                         size_t stride,
                         std::function<void(size_t)> callback);

void ScanMemCompareGreaterAvx2_Int32_Impl(std::span<const std::byte> buf1,
                                          std::span<const std::byte> buf2,
                                          std::function<void(size_t)> callback);

void ScanMemCompareGreaterAvx2_Float_Impl(std::span<const std::byte> buf1,
                                          std::span<const std::byte> buf2,
                                          std::function<void(size_t)> callback);

}  // namespace internal

/// \brief Scans a memory buffer for a pattern, utilizing SIMD if available.
/// \param alignment Only report matches at offsets divisible by this value.
///                  Use sizeof(T) to align scans to data type boundaries.
template <typename Callback>
void ScanBuffer(std::span<const std::byte> buffer,
                std::span<const std::byte> pattern,
                size_t alignment,
                Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if (kHasAvx2) {
    internal::ScanBufferAvx2_Impl(
        buffer, pattern, alignment, std::forward<Callback>(callback));
  } else {
    internal::ScanBufferScalar(
        buffer, pattern, alignment, std::forward<Callback>(callback));
  }
}

namespace internal {

void ScanBufferMaskedAvx2_Impl(std::span<const std::byte> buffer,
                               std::span<const std::byte> pattern,
                               std::span<const std::byte> mask,
                               std::function<void(size_t)> callback);

template <typename Callback>
void ScanBufferMaskedScalar(std::span<const std::byte> buffer,
                            std::span<const std::byte> pattern,
                            std::span<const std::byte> mask,
                            Callback&& callback) {
  if (pattern.empty() || buffer.size() < pattern.size() ||
      mask.size() < pattern.size()) {
    return;
  }

  const size_t pattern_size = pattern.size();
  const size_t limit = buffer.size() - pattern_size;
  const std::byte* buf_ptr = buffer.data();
  const std::byte* pat_ptr = pattern.data();
  const std::byte* mask_ptr = mask.data();

  for (size_t offset = 0; offset <= limit; ++offset) {
    bool match = true;
    for (size_t i = 0; i < pattern_size; ++i) {
      if ((buf_ptr[offset + i] & mask_ptr[i]) != (pat_ptr[i] & mask_ptr[i])) {
        match = false;
        break;
      }
    }
    if (match) {
      callback(offset);
    }
  }
}

}  // namespace internal

/// \brief Scans a memory buffer for a masked pattern.
/// \param mask Must be same size as pattern. 0xFF checks byte, 0x00 ignores.
template <typename Callback>
void ScanBufferMasked(std::span<const std::byte> buffer,
                      std::span<const std::byte> pattern,
                      std::span<const std::byte> mask,
                      Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if (kHasAvx2) {
    internal::ScanBufferMaskedAvx2_Impl(
        buffer, pattern, mask, std::forward<Callback>(callback));
  } else {
    internal::ScanBufferMaskedScalar(
        buffer, pattern, mask, std::forward<Callback>(callback));
  }
}

/// \brief Scans a memory buffer for a pattern (unaligned, byte-level).
/// \deprecated Prefer the aligned overload for type-safe scanning.
template <typename Callback>
void ScanBuffer(std::span<const std::byte> buffer,
                std::span<const std::byte> pattern,
                Callback&& callback) {
  ScanBuffer(buffer, pattern, 1, std::forward<Callback>(callback));
}

/// \brief Scans two buffers for equality/inequality.
template <typename Callback>
void ScanMemCmp(std::span<const std::byte> buf1,
                std::span<const std::byte> buf2,
                bool find_equal,
                size_t stride,
                Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if (kHasAvx2) {
    internal::ScanMemCmpAvx2_Impl(
        buf1, buf2, find_equal, stride, std::forward<Callback>(callback));
  } else {
    internal::ScanMemCmpScalar(
        buf1, buf2, find_equal, stride, std::forward<Callback>(callback));
  }
}

/// \brief Scans for values where buf1[i] > buf2[i].
template <typename T, typename Callback>
void ScanMemCompareGreater(std::span<const std::byte> buf1,
                           std::span<const std::byte> buf2,
                           Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if constexpr (std::is_same_v<T, int32_t>) {
    if (kHasAvx2) {
      internal::ScanMemCompareGreaterAvx2_Int32_Impl(
          buf1, buf2, std::forward<Callback>(callback));
      return;
    }
  } else if constexpr (std::is_same_v<T, float>) {
    if (kHasAvx2) {
      internal::ScanMemCompareGreaterAvx2_Float_Impl(
          buf1, buf2, std::forward<Callback>(callback));
      return;
    }
  }

  internal::ScanMemCompareGreaterScalar<T>(
      buf1, buf2, std::forward<Callback>(callback));
}

}  // namespace maia::core
