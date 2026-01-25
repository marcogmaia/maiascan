// Copyright (c) Maia

#include "maia/core/simd_scanner.h"

#include <bit>
#include <cstring>
#include <functional>

#include "maia/assert.h"

// Platform-specific headers for SIMD
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <immintrin.h>
#endif

// Attribute macro for GCC/Clang to enable AVX2 for specific functions
#if defined(__GNUC__) || defined(__clang__)
#define MAIA_TARGET_AVX2 __attribute__((target("avx2")))
#else
#define MAIA_TARGET_AVX2
#endif

namespace maia::core::internal {

namespace {

// Precomputed bitmasks for common alignments.
// For alignment N, only bits at positions 0, N, 2N, ... are set.
// E.g., alignment 4 -> 0x11111111 (bits 0, 4, 8, 12, 16, 20, 24, 28).
constexpr unsigned int ComputeAlignmentMask(size_t alignment) {
  if (alignment == 1) {
    return 0xFFFFFFFFu;
  }
  if (alignment == 2) {
    return 0x55555555u;  // 0101...
  }
  if (alignment == 4) {
    return 0x11111111u;  // 00010001...
  }
  if (alignment == 8) {
    return 0x01010101u;  // 00000001...
  }
  if (alignment == 16) {
    return 0x00010001u;
  }
  if (alignment == 32) {
    return 0x00000001u;
  }
  // Fallback for unusual alignments: compute dynamically.
  unsigned int mask = 0;
  for (size_t i = 0; i < 32; i += alignment) {
    mask |= (1u << i);
  }
  return mask;
}

}  // namespace

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)

MAIA_TARGET_AVX2 void ScanBufferAvx2_Impl(
    std::span<const std::byte> buffer,
    std::span<const std::byte> pattern,
    size_t alignment,
    std::function<void(size_t)> callback) {
  if (buffer.size() < 32 || pattern.empty()) {
    // Fall back to scalar for small buffers.
    if (pattern.empty() || buffer.size() < pattern.size()) {
      return;
    }
    const size_t pattern_size = pattern.size();
    const size_t limit = buffer.size() - pattern_size;
    for (size_t offset = 0; offset <= limit; offset += alignment) {
      if (std::memcmp(buffer.data() + offset, pattern.data(), pattern_size) ==
          0) {
        callback(offset);
      }
    }
    return;
  }

  const size_t buffer_size = buffer.size();
  const size_t pattern_size = pattern.size();
  const std::byte first_byte = pattern[0];

  const __m256i v_first = _mm256_set1_epi8(static_cast<char>(first_byte));
  const char* buf_ptr = reinterpret_cast<const char*>(buffer.data());
  const char* pat_ptr = reinterpret_cast<const char*>(pattern.data());

  // Precompute alignment mask for this scan.
  const unsigned int alignment_mask = ComputeAlignmentMask(alignment);

  size_t i = 0;
  for (; i <= buffer_size - 32; i += 32) {
    __m256i v_data =
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buf_ptr + i));
    __m256i v_cmp = _mm256_cmpeq_epi8(v_data, v_first);
    unsigned int mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_cmp));

    // Apply alignment filter: only keep bits at aligned offsets.
    // We need to account for the base offset 'i' modulo alignment.
    // If i is aligned (which it always is for power-of-2 alignments <= 32),
    // then the mask is simply AND'd with the precomputed alignment mask.
    // For non-power-of-2 or i not aligned, we'd need to rotate the mask.
    // Since i increments by 32 (always aligned), we can use the static mask.
    mask &= alignment_mask;

    while (mask != 0) {
      int bit_index = std::countr_zero(mask);
      size_t potential_match_offset = i + bit_index;

      if (potential_match_offset + pattern_size <= buffer_size) {
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
      mask &= ~(1u << bit_index);
    }
  }

  // Handle tail bytes with scalar loop.
  if (i < buffer_size && i + pattern_size <= buffer_size) {
    const size_t limit = buffer_size - pattern_size;
    for (size_t offset = i; offset <= limit; offset += alignment) {
      if (std::memcmp(buf_ptr + offset, pat_ptr, pattern_size) == 0) {
        callback(offset);
      }
    }
  }
}

MAIA_TARGET_AVX2 void ScanMemCmpAvx2_Impl(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    bool find_equal,
    size_t stride,
    std::function<void(size_t)> callback) {
  maia::Assert(stride > 0 && stride <= 32, "Stride must be between 1 and 32");
  const size_t size = std::min(buf1.size(), buf2.size());
  if (size < 32) {
    ScanMemCmpScalar(buf1, buf2, find_equal, stride, std::move(callback));
    return;
  }

  const char* p1 = reinterpret_cast<const char*>(buf1.data());
  const char* p2 = reinterpret_cast<const char*>(buf2.data());

  const unsigned int full_stride_mask =
      (stride == 32) ? 0xFFFFFFFFu : ((1u << stride) - 1);

  size_t i = 0;
  for (; i <= size - 32; i += 32) {
    __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p1 + i));
    __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p2 + i));

    __m256i v_eq = _mm256_cmpeq_epi8(v1, v2);
    unsigned int mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_eq));

    if (find_equal) {
      if (mask == 0xFFFFFFFF) {
        for (size_t k = 0; k < 32; k += stride) {
          callback(i + k);
        }
        continue;
      }

      for (size_t k = 0; k <= 32 - stride; k += stride) {
        unsigned int submask = (mask >> k) & full_stride_mask;
        if (submask == full_stride_mask) {
          callback(i + k);
        }
      }
    } else {
      if (mask == 0) {
        for (size_t k = 0; k < 32; k += stride) {
          callback(i + k);
        }
        continue;
      }

      for (size_t k = 0; k <= 32 - stride; k += stride) {
        unsigned int submask = (mask >> k) & full_stride_mask;
        if (submask != full_stride_mask) {
          callback(i + k);
        }
      }
    }
  }

  if (i < size) {
    std::span<const std::byte> t1 = buf1.subspan(i);
    std::span<const std::byte> t2 = buf2.subspan(i);
    ScanMemCmpScalar(t1, t2, find_equal, stride, [&](size_t offset) {
      callback(i + offset);
    });
  }
}

MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2_Int32_Impl(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    std::function<void(size_t)> callback) {
  const size_t stride = sizeof(int32_t);
  const size_t size = std::min(buf1.size(), buf2.size());

  if (size < 32) {
    ScanMemCompareGreaterScalar<int32_t>(buf1, buf2, std::move(callback));
    return;
  }

  const char* p1 = reinterpret_cast<const char*>(buf1.data());
  const char* p2 = reinterpret_cast<const char*>(buf2.data());

  size_t i = 0;
  for (; i <= size - 32; i += 32) {
    __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p1 + i));
    __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p2 + i));

    __m256i v_cmp = _mm256_cmpgt_epi32(v1, v2);
    unsigned int mask = static_cast<unsigned int>(
        _mm256_movemask_ps(_mm256_castsi256_ps(v_cmp)));

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
    ScanMemCompareGreaterScalar<int32_t>(
        t1, t2, [&](size_t offset) { callback(i + offset); });
  }
}

MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2_Float_Impl(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    std::function<void(size_t)> callback) {
  const size_t stride = sizeof(float);
  const size_t size = std::min(buf1.size(), buf2.size());

  if (size < 32) {
    ScanMemCompareGreaterScalar<float>(buf1, buf2, std::move(callback));
    return;
  }

  const char* p1 = reinterpret_cast<const char*>(buf1.data());
  const char* p2 = reinterpret_cast<const char*>(buf2.data());

  size_t i = 0;
  for (; i <= size - 32; i += 32) {
    __m256 v1 = _mm256_loadu_ps(reinterpret_cast<const float*>(p1 + i));
    __m256 v2 = _mm256_loadu_ps(reinterpret_cast<const float*>(p2 + i));

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

}  // namespace maia::core::internal
