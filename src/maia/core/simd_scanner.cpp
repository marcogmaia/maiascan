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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)

MAIA_TARGET_AVX2 void ScanBufferAvx2_Impl(
    std::span<const std::byte> buffer,
    std::span<const std::byte> pattern,
    std::function<void(size_t)> callback) {
  if (buffer.size() < 32 || pattern.empty()) {
    ScanBufferScalar(buffer, pattern, std::move(callback));
    return;
  }

  const size_t buffer_size = buffer.size();
  const size_t pattern_size = pattern.size();
  const std::byte first_byte = pattern[0];

  const __m256i v_first = _mm256_set1_epi8(static_cast<char>(first_byte));
  const char* buf_ptr = reinterpret_cast<const char*>(buffer.data());
  const char* pat_ptr = reinterpret_cast<const char*>(pattern.data());

  size_t i = 0;
  for (; i <= buffer_size - 32; i += 32) {
    __m256i v_data =
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buf_ptr + i));
    __m256i v_cmp = _mm256_cmpeq_epi8(v_data, v_first);
    unsigned int mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_cmp));

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

  if (i < buffer_size) {
    std::span<const std::byte> tail = buffer.subspan(i);
    ScanBufferScalar(
        tail, pattern, [&](size_t offset) { callback(i + offset); });
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
