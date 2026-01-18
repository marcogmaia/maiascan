// Copyright (c) Maia

#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>

// Platform-specific headers for SIMD
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <immintrin.h>
#endif

#include "maia/core/cpu_info.h"

// Attribute macro for GCC/Clang to enable AVX2 for specific functions
#if defined(__GNUC__) || defined(__clang__)
#define MAIA_TARGET_AVX2 __attribute__((target("avx2")))
#else
#define MAIA_TARGET_AVX2
#endif

namespace maia::core {

namespace internal {

/// \brief Standard library search fallback for scalar execution.
template <typename Callback>
void ScanBufferScalar(std::span<const std::byte> buffer,
                      std::span<const std::byte> pattern,
                      Callback&& callback) {
  auto it = buffer.begin();
  const auto end = buffer.end();
  const auto pat_begin = pattern.begin();
  const auto pat_end = pattern.end();

  while (true) {
    it = std::search(it, end, pat_begin, pat_end);
    if (it == end) {
      break;
    }

    size_t offset = std::distance(buffer.begin(), it);
    callback(offset);

    std::advance(it, 1);
  }
}

/// \brief AVX2 optimized scan implementation.
/// \details Uses _mm256_cmpeq_epi8 to scan 32 bytes at a time.
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
template <typename Callback>
MAIA_TARGET_AVX2 void ScanBufferAvx2(std::span<const std::byte> buffer,
                                     std::span<const std::byte> pattern,
                                     Callback&& callback) {
  if (buffer.size() < 32 || pattern.empty()) {
    ScanBufferScalar(buffer, pattern, std::forward<Callback>(callback));
    return;
  }

  const size_t buffer_size = buffer.size();
  const size_t pattern_size = pattern.size();
  const std::byte first_byte = pattern[0];

  // Create vector with the first byte of the pattern replicated 32 times
  const __m256i v_first = _mm256_set1_epi8(static_cast<char>(first_byte));

  const char* buf_ptr = reinterpret_cast<const char*>(buffer.data());
  const char* pat_ptr = reinterpret_cast<const char*>(pattern.data());

  size_t i = 0;
  // Process 32 bytes at a time
  for (; i <= buffer_size - 32; i += 32) {
    // Load 32 bytes from buffer
    __m256i v_data =
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buf_ptr + i));

    // Compare with first byte
    __m256i v_cmp = _mm256_cmpeq_epi8(v_data, v_first);

    // Create mask from comparison result
    unsigned int mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_cmp));

    while (mask != 0) {
      // Get index of the first set bit (number of trailing zeros)
      int bit_index = std::countr_zero(mask);

      // Calculate actual offset
      size_t potential_match_offset = i + bit_index;

      // Check boundary condition: if pattern extends beyond buffer end
      if (potential_match_offset + pattern_size <= buffer_size) {
        // Verify the rest of the pattern
        // We skip the first byte comparison since SIMD already confirmed it
        bool full_match = true;
        if (pattern_size > 1) {
          full_match = (std::memcmp(buf_ptr + potential_match_offset + 1,
                                    pat_ptr + 1,
                                    pattern_size - 1) == 0);
        }

        if (full_match) {
          callback(potential_match_offset);
        }
      }

      // Clear the processed bit
      mask &= ~(1u << bit_index);
    }
  }

  // Handle remaining bytes with scalar fallback
  if (i < buffer_size) {
    std::span<const std::byte> tail = buffer.subspan(i);
    // We must adjust the callback to add the offset 'i'
    ScanBufferScalar(
        tail, pattern, [&](size_t offset) { callback(i + offset); });
  }
}
#else
// Fallback for non-x86 architectures
template <typename Callback>
void ScanBufferAvx2(std::span<const std::byte> buffer,
                    std::span<const std::byte> pattern,
                    Callback&& callback) {
  ScanBufferScalar(buffer, pattern, std::forward<Callback>(callback));
}
#endif

}  // namespace internal

/// \brief Scans a memory buffer for a pattern, utilizing SIMD if available.
/// \param buffer The memory buffer to scan (haystack).
/// \param pattern The pattern to search for (needle).
/// \param callback Function to call for each match found (receives offset).
template <typename Callback>
void ScanBuffer(std::span<const std::byte> buffer,
                std::span<const std::byte> pattern,
                Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();

  if (kHasAvx2) {
    internal::ScanBufferAvx2(buffer, pattern, std::forward<Callback>(callback));
  } else {
    internal::ScanBufferScalar(
        buffer, pattern, std::forward<Callback>(callback));
  }
}

namespace internal {

template <typename Callback>
void ScanMemCmpScalar(std::span<const std::byte> buf1,
                      std::span<const std::byte> buf2,
                      bool find_equal,
                      size_t stride,
                      Callback&& callback) {
  const size_t size = std::min(buf1.size(), buf2.size());
  const size_t limit = size - (size % stride);  // Round down to stride

  for (size_t i = 0; i < limit; i += stride) {
    bool equal = (std::memcmp(&buf1[i], &buf2[i], stride) == 0);
    if (equal == find_equal) {
      callback(i);
    }
  }
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
template <typename Callback>
MAIA_TARGET_AVX2 void ScanMemCmpAvx2(std::span<const std::byte> buf1,
                                     std::span<const std::byte> buf2,
                                     bool find_equal,
                                     size_t stride,
                                     Callback&& callback) {
  const size_t size = std::min(buf1.size(), buf2.size());
  if (size < 32) {
    ScanMemCmpScalar(
        buf1, buf2, find_equal, stride, std::forward<Callback>(callback));
    return;
  }

  const char* p1 = reinterpret_cast<const char*>(buf1.data());
  const char* p2 = reinterpret_cast<const char*>(buf2.data());

  size_t i = 0;
  for (; i <= size - 32; i += 32) {
    __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p1 + i));
    __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p2 + i));

    // 0xFF where equal, 0x00 where different
    __m256i v_eq = _mm256_cmpeq_epi8(v1, v2);
    unsigned int mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_eq));

    if (find_equal) {
      // Logic: For stride N, we need N consecutive 1s.
      if (mask == 0xFFFFFFFF) {
        for (size_t k = 0; k < 32; k += stride) {
          callback(i + k);
        }
        continue;
      }

      for (size_t k = 0; k < 32; k += stride) {
        unsigned int submask = (mask >> k) & ((1u << stride) - 1);
        if (submask == ((1u << stride) - 1)) {
          callback(i + k);
        }
      }
    } else {
      // Logic: For stride N, we need ANY 0.
      if (mask == 0) {
        for (size_t k = 0; k < 32; k += stride) {
          callback(i + k);
        }
        continue;
      }

      for (size_t k = 0; k < 32; k += stride) {
        unsigned int submask = (mask >> k) & ((1u << stride) - 1);
        if (submask != ((1u << stride) - 1)) {
          callback(i + k);
        }
      }
    }
  }

  // Tail
  if (i < size) {
    std::span<const std::byte> t1 = buf1.subspan(i);
    std::span<const std::byte> t2 = buf2.subspan(i);
    ScanMemCmpScalar(t1, t2, find_equal, stride, [&](size_t offset) {
      callback(i + offset);
    });
  }
}
#else
template <typename Callback>
void ScanMemCmpAvx2(std::span<const std::byte> buf1,
                    std::span<const std::byte> buf2,
                    bool find_equal,
                    size_t stride,
                    Callback&& callback) {
  ScanMemCmpScalar(
      buf1, buf2, find_equal, stride, std::forward<Callback>(callback));
}
#endif

}  // namespace internal

/// \brief Scans two buffers for equality/inequality.
template <typename Callback>
void ScanMemCmp(std::span<const std::byte> buf1,
                std::span<const std::byte> buf2,
                bool find_equal,
                size_t stride,
                Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();

  if (kHasAvx2) {
    internal::ScanMemCmpAvx2(
        buf1, buf2, find_equal, stride, std::forward<Callback>(callback));
  } else {
    internal::ScanMemCmpScalar(
        buf1, buf2, find_equal, stride, std::forward<Callback>(callback));
  }
}

namespace internal {

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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)

// AVX2 specialization for int32_t
template <typename Callback>
MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2_Int32(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    Callback&& callback) {
  const size_t stride = sizeof(int32_t);
  const size_t size = std::min(buf1.size(), buf2.size());

  if (size < 32) {
    ScanMemCompareGreaterScalar<int32_t>(
        buf1, buf2, std::forward<Callback>(callback));
    return;
  }

  const char* p1 = reinterpret_cast<const char*>(buf1.data());
  const char* p2 = reinterpret_cast<const char*>(buf2.data());

  size_t i = 0;
  for (; i <= size - 32; i += 32) {
    __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p1 + i));
    __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p2 + i));

    // Compare Greater Than (v1 > v2) for 32-bit integers
    __m256i v_cmp = _mm256_cmpgt_epi32(v1, v2);
    unsigned int mask = static_cast<unsigned int>(
        _mm256_movemask_ps(_mm256_castsi256_ps(v_cmp)));

    while (mask != 0) {
      int bit_index = std::countr_zero(mask);
      // bit_index corresponds to the index of the 32-bit integer within the
      // 256-bit vector (0-7) Actual byte offset = i + (bit_index * stride)
      size_t offset = i + (bit_index * stride);
      callback(offset);
      mask &= ~(1u << bit_index);
    }
  }

  if (i < size) {
    std::span<const std::byte> t1 = buf1.subspan(i);
    std::span<const std::byte> t2 = buf2.subspan(i);
    ScanMemCompareGreaterScalar<int32_t>(
        t1, t2, [&](size_t offset) { callback(i + offset); });
  }
}

// AVX2 specialization for float
template <typename Callback>
MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2_Float(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    Callback&& callback) {
  const size_t stride = sizeof(float);
  const size_t size = std::min(buf1.size(), buf2.size());

  if (size < 32) {
    ScanMemCompareGreaterScalar<float>(
        buf1, buf2, std::forward<Callback>(callback));
    return;
  }

  const char* p1 = reinterpret_cast<const char*>(buf1.data());
  const char* p2 = reinterpret_cast<const char*>(buf2.data());

  size_t i = 0;
  for (; i <= size - 32; i += 32) {
    __m256 v1 = _mm256_loadu_ps(reinterpret_cast<const float*>(p1 + i));
    __m256 v2 = _mm256_loadu_ps(reinterpret_cast<const float*>(p2 + i));

    // Compare Ordered Greater Than
    __m256 v_cmp = _mm256_cmp_ps(v1, v2, _CMP_GT_OQ);
    unsigned int mask = static_cast<unsigned int>(_mm256_movemask_ps(v_cmp));

    while (mask != 0) {
      int bit_index = std::countr_zero(mask);
      size_t offset = i + (bit_index * stride);
      callback(offset);
      mask &= ~(1u << bit_index);
    }
  }

  if (i < size) {
    std::span<const std::byte> t1 = buf1.subspan(i);
    std::span<const std::byte> t2 = buf2.subspan(i);
    ScanMemCompareGreaterScalar<float>(
        t1, t2, [&](size_t offset) { callback(i + offset); });
  }
}

#endif

}  // namespace internal

/// \brief Scans for values where buf1[i] > buf2[i].
template <typename T, typename Callback>
void ScanMemCompareGreater(std::span<const std::byte> buf1,
                           std::span<const std::byte> buf2,
                           Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
  if constexpr (std::is_same_v<T, int32_t>) {
    if (kHasAvx2) {
      internal::ScanMemCompareGreaterAvx2_Int32(
          buf1, buf2, std::forward<Callback>(callback));
      return;
    }
  } else if constexpr (std::is_same_v<T, float>) {
    if (kHasAvx2) {
      internal::ScanMemCompareGreaterAvx2_Float(
          buf1, buf2, std::forward<Callback>(callback));
      return;
    }
  }
#endif

  // Fallback for other types or non-AVX2
  internal::ScanMemCompareGreaterScalar<T>(
      buf1, buf2, std::forward<Callback>(callback));
}

}  // namespace maia::core
