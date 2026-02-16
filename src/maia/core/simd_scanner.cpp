// Copyright (c) Maia

#include "maia/core/simd_scanner.h"

#include <bit>
#include <cstring>
#include <functional>

#include "maia/assert.h"

#ifdef _MSC_VER
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <immintrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MAIA_TARGET_AVX2 __attribute__((target("avx2")))
#else
#define MAIA_TARGET_AVX2
#endif

namespace maia::core::internal {

namespace {

// AVX2 operates on 256-bit (32-byte) registers. Processing in chunks of this
// size maximizes throughput by fully utilizing SIMD parallelization.
constexpr size_t kAvx2RegisterBytes = 32;

// Maximum lane count for byte-wise operations in a 256-bit AVX2 register.
constexpr size_t kAvx2LaneCount = 32;

// Represents all 32 bits set (0xFFFFFFFF). Used for mask comparisons where
// we need to check if all comparison lanes matched.
constexpr unsigned int kAllBitsSet = 0xFFFFFFFFu;

// Computes a bitmask where bits are set at positions that match the requested
// alignment. For example, alignment=4 sets bits at positions 0, 4, 8, ... 28.
// This allows efficient filtering of match positions using simple bitwise AND.
constexpr unsigned int ComputeAlignmentMask(size_t alignment) {
  // clang-format off
  if (alignment == 1)  { return 0xFFFFFFFFu; }
  if (alignment == 2)  { return 0x55555555u; }
  if (alignment == 4)  { return 0x11111111u; }
  if (alignment == 8)  { return 0x01010101u; }
  if (alignment == 16) { return 0x00010001u; }
  if (alignment == 32) { return 0x00000001u; }
  // clang-format on
  unsigned int mask = 0;
  for (size_t i = 0; i < kAvx2LaneCount; i += alignment) {
    mask |= (1u << i);
  }
  return mask;
}

// TODO(marco): Add safe types conversions as requires/concepts.
// Type-safe wrapper around reinterpret_cast for byte buffer access.
// Required because SIMD intrinsics need specific pointer types (e.g., __m256i*,
// float*) but we receive std::byte buffers. The reinterpret_cast is safe here
// because we control the buffer alignment and size at the call sites.
template <typename T>
const T* As(const std::byte* ptr) {
  return reinterpret_cast<const T*>(ptr);  // NOLINT
}

__m256i LoadAvx2(const std::byte* ptr) {
  return _mm256_loadu_si256(As<__m256i>(ptr));
}

__m256i LoadAvx2Aligned(const std::byte* ptr) {
  return _mm256_load_si256(As<__m256i>(ptr));
}

__m256 LoadAvx2Float(const float* ptr) {
  return _mm256_loadu_ps(ptr);
}

// Type traits for AVX2 greater-than comparison operations.
// Provides a unified interface for different numeric types (int32, float).
// This allows a single template implementation for comparison scans.
template <typename T>
struct Avx2CompareTraits;

template <>
struct Avx2CompareTraits<int32_t> {
  using VecType = __m256i;
  static constexpr size_t kElementSize = sizeof(int32_t);

  static VecType Load(const std::byte* ptr) {
    return LoadAvx2(ptr);
  }

  static VecType CompareGreater(VecType a, VecType b) {
    return _mm256_cmpgt_epi32(a, b);
  }

  static unsigned int ExtractMask(VecType cmp_result) {
    // cmpgt_epi32 returns int32 mask, need to reinterpret as float for movemask
    return static_cast<unsigned int>(
        _mm256_movemask_ps(_mm256_castsi256_ps(cmp_result)));
  }
};

template <>
struct Avx2CompareTraits<float> {
  using VecType = __m256;
  static constexpr size_t kElementSize = sizeof(float);

  static VecType Load(const std::byte* ptr) {
    return LoadAvx2Float(As<float>(ptr));
  }

  static VecType CompareGreater(VecType a, VecType b) {
    return _mm256_cmp_ps(a, b, _CMP_GT_OQ);
  }

  static unsigned int ExtractMask(VecType cmp_result) {
    return static_cast<unsigned int>(_mm256_movemask_ps(cmp_result));
  }
};

bool VerifyFullMatch(const std::byte* buf_ptr,
                     const std::byte* pat_ptr,
                     size_t match_offset,
                     size_t buffer_size,
                     size_t pattern_size) {
  if (match_offset + pattern_size > buffer_size) {
    return false;
  }
  if (pattern_size > 1) {
    return std::memcmp(
               buf_ptr + match_offset + 1, pat_ptr + 1, pattern_size - 1) == 0;
  }
  return true;
}

bool VerifyMaskedMatchAvx2(const std::byte* buf_ptr,
                           const __m256i v_mask,
                           const __m256i v_pat_masked,
                           size_t offset,
                           size_t buffer_size,
                           size_t pattern_size) {
  // Ensure we have enough data to load a full AVX2 register starting at offset.
  if (offset + pattern_size > buffer_size ||
      offset + kAvx2RegisterBytes > buffer_size) {
    return false;
  }
  __m256i v_candidate = LoadAvx2(buf_ptr + offset);
  __m256i v_candidate_masked = _mm256_and_si256(v_candidate, v_mask);
  __m256i v_final_cmp = _mm256_cmpeq_epi8(v_candidate_masked, v_pat_masked);
  auto final_mask =
      static_cast<unsigned int>(_mm256_movemask_epi8(v_final_cmp));
  unsigned int needed_bits = (pattern_size == kAvx2RegisterBytes)
                                 ? kAllBitsSet
                                 : ((1u << pattern_size) - 1);
  return (final_mask & needed_bits) == needed_bits;
}

bool VerifyMaskedMatchScalar(const std::byte* buf_ptr,
                             const std::byte* pat_ptr,
                             const std::byte* mask_ptr,
                             size_t offset,
                             size_t buffer_size,
                             size_t pattern_size) {
  if (offset + pattern_size > buffer_size) {
    return false;
  }
  for (size_t k = 0; k < pattern_size; ++k) {
    if ((buf_ptr[offset + k] & mask_ptr[k]) != (pat_ptr[k] & mask_ptr[k])) {
      return false;
    }
  }
  return true;
}

// Reports matches at all strided positions within a 32-byte AVX2 register.
// Used when we know all positions match (fast path optimization).
void ReportAllMatches(size_t base_offset,
                      size_t stride,
                      std::function<void(size_t)> callback) {
  for (size_t k = 0; k < kAvx2LaneCount; k += stride) {
    callback(base_offset + k);
  }
}

// Processes matches for the "find equal" case where we want positions where
// all stride bytes match. Uses early return for the fast path where all lanes
// match.
void ProcessEqualMatches(size_t base_offset,
                         unsigned int mask,
                         unsigned int full_stride_mask,
                         size_t stride,
                         std::function<void(size_t)> callback) {
  // Fast path: all 32 bytes match, report all strided positions immediately.
  if (mask == kAllBitsSet) {
    ReportAllMatches(base_offset, stride, callback);
    return;
  }

  // Check each strided position. A position matches if all stride bytes match.
  for (size_t k = 0; k <= kAvx2LaneCount - stride; k += stride) {
    unsigned int submask = (mask >> k) & full_stride_mask;
    if (submask == full_stride_mask) {
      callback(base_offset + k);
    }
  }
}

// Processes matches for the "find different" case where we want positions where
// at least one byte differs. Uses early return for the fast path where no lanes
// match.
void ProcessDifferentMatches(size_t base_offset,
                             unsigned int mask,
                             unsigned int full_stride_mask,
                             size_t stride,
                             std::function<void(size_t)> callback) {
  // Fast path: no bytes match, all strided positions differ.
  if (mask == 0) {
    ReportAllMatches(base_offset, stride, callback);
    return;
  }

  // Check each strided position. A position differs if any byte differs.
  for (size_t k = 0; k <= kAvx2LaneCount - stride; k += stride) {
    unsigned int submask = (mask >> k) & full_stride_mask;
    if (submask != full_stride_mask) {
      callback(base_offset + k);
    }
  }
}

// Dispatches to the appropriate match processor based on whether we're looking
// for equal or different values. Keeps the main logic flat by delegating.
void ProcessMatches(size_t base_offset,
                    unsigned int mask,
                    unsigned int full_stride_mask,
                    size_t stride,
                    bool find_equal,
                    std::function<void(size_t)> callback) {
  if (find_equal) {
    ProcessEqualMatches(base_offset, mask, full_stride_mask, stride, callback);
  } else {
    ProcessDifferentMatches(
        base_offset, mask, full_stride_mask, stride, callback);
  }
}

void ScanTail(const std::byte* buf_ptr,
              const std::byte* pat_ptr,
              size_t buffer_size,
              size_t pattern_size,
              size_t start_offset,
              size_t alignment,
              std::function<void(size_t)> callback) {
  if (start_offset >= buffer_size ||
      start_offset + pattern_size > buffer_size) {
    return;
  }
  // Scan the tail region, adjusting reported offsets to be relative to the
  // original buffer start (not the tail subspan).
  ScanBufferScalar(
      std::span(buf_ptr + start_offset, buffer_size - start_offset),
      std::span(pat_ptr, pattern_size),
      alignment,
      [&](size_t offset) { callback(start_offset + offset); });
}

// Processes the tail (remaining bytes after AVX2 loop) using scalar comparison.
// Template allows reuse across different comparison types (equal, greater,
// etc).
template <typename ScalarFn>
void ProcessAvx2Tail(std::span<const std::byte> buf1,
                     std::span<const std::byte> buf2,
                     size_t processed_bytes,
                     ScalarFn&& scalar_fn,
                     std::function<void(size_t)> callback) {
  if (processed_bytes >= buf1.size() || processed_bytes >= buf2.size()) {
    return;
  }
  // Adjust offsets to be relative to the original buffer start.
  std::forward<ScalarFn>(scalar_fn)(
      buf1.subspan(processed_bytes),
      buf2.subspan(processed_bytes),
      [&](size_t offset) { callback(processed_bytes + offset); });
}

}  // namespace

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)

MAIA_TARGET_AVX2 void ScanBufferAvx2(std::span<const std::byte> buffer,
                                     std::span<const std::byte> pattern,
                                     size_t alignment,
                                     std::function<void(size_t)> callback) {
  const size_t buffer_size = buffer.size();
  const size_t pattern_size = pattern.size();

  // AVX2 requires at least a full register of data to be efficient. For smaller
  // buffers, the overhead of SIMD setup outweighs the benefits.
  if (buffer_size < kAvx2RegisterBytes || pattern.empty()) {
    if (pattern.empty() || buffer_size < pattern_size) {
      return;
    }
    ScanBufferScalar(buffer, pattern, alignment, callback);
    return;
  }

  const __m256i v_first = _mm256_set1_epi8(static_cast<char>(pattern[0]));
  const auto* buf_ptr = buffer.data();
  const auto* pat_ptr = pattern.data();
  const unsigned int alignment_mask = ComputeAlignmentMask(alignment);

  // Process buffer in 32-byte chunks. The loop condition ensures we don't
  // read past the end, leaving any remainder for scalar tail processing.
  size_t i = 0;
  for (; i <= buffer_size - kAvx2RegisterBytes; i += kAvx2RegisterBytes) {
    __m256i v_data = LoadAvx2(buf_ptr + i);
    __m256i v_cmp = _mm256_cmpeq_epi8(v_data, v_first);
    auto mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_cmp));
    mask &= alignment_mask;

    while (mask != 0) {
      int bit_index = std::countr_zero(mask);
      size_t potential_match_offset = i + bit_index;

      if (VerifyFullMatch(buf_ptr,
                          pat_ptr,
                          potential_match_offset,
                          buffer_size,
                          pattern_size)) {
        callback(potential_match_offset);
      }
      mask &= ~(1u << bit_index);
    }
  }

  ScanTail(buf_ptr, pat_ptr, buffer_size, pattern_size, i, alignment, callback);
}

MAIA_TARGET_AVX2 void ScanBufferMaskedAvx2(
    std::span<const std::byte> buffer,
    std::span<const std::byte> pattern,
    std::span<const std::byte> mask,
    std::function<void(size_t)> callback) {
  const size_t buffer_size = buffer.size();
  const size_t pattern_size = pattern.size();

  if (buffer_size < pattern_size || pattern.empty() || mask.empty()) {
    return;
  }

  // Pattern must fit in a single AVX2 register for SIMD comparison.
  // Larger patterns require more complex multi-register logic.
  if (buffer_size < kAvx2RegisterBytes || pattern_size > kAvx2RegisterBytes) {
    maia::core::internal::ScanBufferMaskedScalar(
        buffer, pattern, mask, callback);
    return;
  }

  alignas(32) std::byte pat_buf[32]{};
  alignas(32) std::byte mask_buf[32]{};
  std::memcpy(pat_buf, pattern.data(), pattern_size);
  std::memcpy(mask_buf, mask.data(), pattern_size);

  const __m256i v_pat = LoadAvx2Aligned(pat_buf);
  const __m256i v_mask = LoadAvx2Aligned(mask_buf);
  const __m256i v_pat_masked = _mm256_and_si256(v_pat, v_mask);

  const auto* buf_ptr = buffer.data();
  const std::byte* pat_ptr = pattern.data();

  size_t i = 0;
  if (mask[0] == std::byte{0xFF}) {
    const __m256i v_first = _mm256_set1_epi8(static_cast<char>(pattern[0]));
    // Process in 32-byte chunks, checking first byte match to quickly filter.
    for (; i <= buffer_size - kAvx2RegisterBytes; i += kAvx2RegisterBytes) {
      __m256i v_data = LoadAvx2(buf_ptr + i);
      __m256i v_cmp = _mm256_cmpeq_epi8(v_data, v_first);
      auto match_mask = static_cast<uint32_t>(_mm256_movemask_epi8(v_cmp));

      while (match_mask != 0) {
        int bit_index = std::countr_zero(match_mask);
        size_t offset = i + bit_index;

        if (VerifyMaskedMatchAvx2(buf_ptr,
                                  v_mask,
                                  v_pat_masked,
                                  offset,
                                  buffer_size,
                                  pattern_size)) {
          callback(offset);
        } else if (VerifyMaskedMatchScalar(buf_ptr,
                                           pat_ptr,
                                           mask.data(),
                                           offset,
                                           buffer_size,
                                           pattern_size)) {
          callback(offset);
        }
        match_mask &= ~(1u << bit_index);
      }
    }
  } else {
    maia::core::internal::ScanBufferMaskedScalar(
        buffer, pattern, mask, callback);
    return;
  }

  if (i < buffer_size && i + pattern_size <= buffer_size) {
    maia::core::internal::ScanBufferMaskedScalar(
        buffer.subspan(i), pattern, mask, [&](size_t off) {
          callback(i + off);
        });
  }
}

MAIA_TARGET_AVX2 void ScanMemCmpAvx2(std::span<const std::byte> buf1,
                                     std::span<const std::byte> buf2,
                                     bool find_equal,
                                     size_t stride,
                                     std::function<void(size_t)> callback) {
  // Stride determines how many bytes we compare at each position.
  // Must fit within a 32-byte AVX2 register (e.g., 4 x 8-byte values).
  maia::Assert(stride > 0 && stride <= kAvx2RegisterBytes,
               "Stride must be between 1 and 32");
  const size_t size = std::min(buf1.size(), buf2.size());

  // For small buffers, scalar comparison is faster (no SIMD setup overhead).
  if (size < kAvx2RegisterBytes) {
    maia::core::internal::ScanMemCmpScalar(
        buf1, buf2, find_equal, stride, callback);
    return;
  }

  // Create a mask with 'stride' low bits set. Used to check if all bytes in
  // a strided position match (e.g., for 4-byte stride, check if bits 0-3 all
  // set).
  const unsigned int full_stride_mask =
      (stride == kAvx2LaneCount) ? kAllBitsSet : ((1u << stride) - 1);

  // Process 32 bytes at a time. Each iteration compares two chunks and reports
  // positions where the comparison matches the requested condition
  // (equal/different).
  size_t i = 0;
  for (; i <= size - kAvx2RegisterBytes; i += kAvx2RegisterBytes) {
    __m256i v1 = LoadAvx2(buf1.data() + i);
    __m256i v2 = LoadAvx2(buf2.data() + i);

    // Compare byte-by-byte, then extract match mask. Each bit represents one
    // byte.
    __m256i v_eq = _mm256_cmpeq_epi8(v1, v2);
    auto mask = static_cast<unsigned int>(_mm256_movemask_epi8(v_eq));

    ProcessMatches(i, mask, full_stride_mask, stride, find_equal, callback);
  }

  // Handle remaining bytes that don't fill a full AVX2 register.
  ProcessAvx2Tail(
      buf1,
      buf2,
      i,
      [&](std::span<const std::byte> tail1,
          std::span<const std::byte> tail2,
          auto&& cb) {
        maia::core::internal::ScanMemCmpScalar(
            tail1, tail2, find_equal, stride, cb);
      },
      callback);
}

namespace {

// Unified greater-than comparison using AVX2.
// Works for any type with Avx2CompareTraits specialization (int32, float).
// Template dispatch allows compiler to inline type-specific operations.
template <typename T>
MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    std::function<void(size_t)> callback) {
  using Traits = Avx2CompareTraits<T>;
  const size_t stride = Traits::kElementSize;
  const size_t size = std::min(buf1.size(), buf2.size());

  // For small buffers, scalar comparison avoids AVX2 setup overhead.
  if (size < kAvx2RegisterBytes) {
    maia::core::internal::ScanMemCompareGreaterScalar<T>(
        buf1, buf2, std::move(callback));
    return;
  }

  // Process one AVX2 register (32 bytes) per iteration.
  // Each iteration compares 8 int32 or 8 float values simultaneously.
  size_t i = 0;
  for (; i <= size - kAvx2RegisterBytes; i += kAvx2RegisterBytes) {
    auto v1 = Traits::Load(buf1.data() + i);
    auto v2 = Traits::Load(buf2.data() + i);

    // Compare pairs element-wise, then extract result mask.
    // Each bit in the mask represents one element comparison.
    auto v_cmp = Traits::CompareGreater(v1, v2);
    auto mask = Traits::ExtractMask(v_cmp);

    // Report each position where buf1[i] > buf2[i].
    while (mask != 0) {
      int bit_index = std::countr_zero(mask);
      size_t offset = i + (bit_index * stride);
      callback(offset);
      mask &= ~(1u << bit_index);
    }
  }

  // Process remaining bytes with scalar comparison.
  ProcessAvx2Tail(
      buf1,
      buf2,
      i,
      [](std::span<const std::byte> tail1,
         std::span<const std::byte> tail2,
         auto&& cb) { ScanMemCompareGreaterScalar<T>(tail1, tail2, cb); },
      callback);
}

}  // namespace

// Public wrappers maintain API compatibility while delegating to unified
// implementation.
MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2Int32(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    std::function<void(size_t)> callback) {
  ScanMemCompareGreaterAvx2<int32_t>(buf1, buf2, std::move(callback));
}

MAIA_TARGET_AVX2 void ScanMemCompareGreaterAvx2Float(
    std::span<const std::byte> buf1,
    std::span<const std::byte> buf2,
    std::function<void(size_t)> callback) {
  ScanMemCompareGreaterAvx2<float>(buf1, buf2, std::move(callback));
}

#endif

}  // namespace maia::core::internal
