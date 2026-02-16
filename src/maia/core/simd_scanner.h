// Copyright (c) Maia

#pragma once

/// \file simd_scanner.h
/// \brief SIMD-accelerated memory scanning operations with scalar fallback.
///
/// Provides pattern matching and buffer comparison functions that automatically
/// utilize AVX2 when available, falling back to scalar implementations on
/// non-AVX2 systems or small buffers.
///
/// All public functions are thread-safe when called with independent buffers.

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <span>
#include <utility>

#include "maia/core/cpu_info.h"

namespace maia::core {

/// \brief Callback type for scan match notifications.
///
/// Called when a scan finds a match. Receives the byte offset where the match
/// begins in the scanned buffer.
///
/// \param offset Byte position of the match (0-based).
///
/// Example:
/// \code
/// ScanBuffer(buffer, pattern, 1, [](size_t offset) {
///   // offset = 42 means pattern found at buffer[42]
/// });
/// \endcode
template <typename F>
concept ScanCallback = std::invocable<F, size_t>;

namespace internal {

/// \brief Scalar fallback for pattern scanning.
///
/// Performs linear search through buffer using std::memcmp for pattern
/// matching. Respects alignment constraint by only checking positions at
/// aligned offsets.
///
/// \param buffer Source data to search.
/// \param pattern Bytes to find. Empty patterns result in no matches.
/// \param alignment Only check positions divisible by this value. Must be > 0.
/// \param callback Invoked for each match with the byte offset.
template <typename Callback>
void ScanBufferScalar(std::span<const std::byte> buffer,
                      std::span<const std::byte> pattern,
                      size_t alignment,
                      const Callback& callback) {
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

/// \brief Scalar comparison between two buffers.
///
/// Compares two buffers byte-by-byte or in strides, reporting positions
/// that are equal or not equal based on find_equal parameter.
///
/// \param buf1 First buffer to compare.
/// \param buf2 Second buffer to compare.
/// \param find_equal If true, report positions where buffers match.
///                   If false, report positions where buffers differ.
/// \param stride Number of bytes to compare at each position (1 to 32).
/// \param callback Invoked for each matching position with byte offset.
template <typename Callback>
void ScanMemCmpScalar(std::span<const std::byte> buf1,
                      std::span<const std::byte> buf2,
                      bool find_equal,
                      size_t stride,
                      const Callback& callback) {
  const size_t size = std::min(buf1.size(), buf2.size());
  const size_t limit = size - (size % stride);

  for (size_t i = 0; i < limit; i += stride) {
    bool equal = (std::memcmp(&buf1[i], &buf2[i], stride) == 0);
    if (equal == find_equal) {
      callback(i);
    }
  }
}

/// \brief Scalar comparison for greater-than values.
///
/// Interprets buffers as arrays of type T and reports positions where
/// buf1[i] > buf2[i].
///
/// \tparam T Numeric type (int32_t, float, etc.). Must be arithmetic.
/// \param buf1 First buffer containing values to compare.
/// \param buf2 Second buffer containing values to compare against.
/// \param callback Invoked for each position where buf1 > buf2.
///                 Receives byte offset (not element index).
template <typename T, typename Callback>
  requires std::is_arithmetic_v<T>
void ScanMemCompareGreaterScalar(std::span<const std::byte> buf1,
                                 std::span<const std::byte> buf2,
                                 const Callback& callback) {
  const size_t count = std::min(buf1.size(), buf2.size()) / sizeof(T);
  const T* val1 = reinterpret_cast<const T*>(buf1.data());  // NOLINT
  const T* val2 = reinterpret_cast<const T*>(buf2.data());  // NOLINT

  for (size_t i = 0; i < count; ++i) {
    if (val1[i] > val2[i]) {
      callback(i * sizeof(T));
    }
  }
}

/// \brief AVX2 implementation for pattern scanning.
///
/// Processes 32 bytes at a time using AVX2 SIMD instructions.
/// Falls back to scalar for small buffers (< 32 bytes).
/// Implemented in simd_scanner.cpp to minimize header size.
///
/// \param buffer Source data to search.
/// \param pattern Bytes to find.
/// \param alignment Only check positions divisible by this value.
/// \param callback Invoked for each match with byte offset.
void ScanBufferAvx2(std::span<const std::byte> buffer,
                    std::span<const std::byte> pattern,
                    size_t alignment,
                    std::function<void(size_t)> callback);

/// \brief AVX2 implementation for buffer comparison.
///
/// Compares two buffers 32 bytes at a time using AVX2 SIMD.
/// Implemented in simd_scanner.cpp.
///
/// \param buf1 First buffer to compare.
/// \param buf2 Second buffer to compare.
/// \param find_equal If true, find matches; if false, find differences.
/// \param stride Comparison granularity in bytes.
/// \param callback Invoked for each matching position.
void ScanMemCmpAvx2(std::span<const std::byte> buf1,
                    std::span<const std::byte> buf2,
                    bool find_equal,
                    size_t stride,
                    std::function<void(size_t)> callback);

/// \brief AVX2 implementation for int32 greater-than comparison.
///
/// Processes 8 int32 values per AVX2 register (32 bytes).
/// Implemented in simd_scanner.cpp.
///
/// \param buf1 First buffer containing int32 values.
/// \param buf2 Second buffer containing int32 values.
/// \param callback Invoked for each position where buf1 > buf2.
void ScanMemCompareGreaterAvx2Int32(std::span<const std::byte> buf1,
                                    std::span<const std::byte> buf2,
                                    std::function<void(size_t)> callback);

/// \brief AVX2 implementation for float greater-than comparison.
///
/// Processes 8 float values per AVX2 register (32 bytes).
/// Implemented in simd_scanner.cpp.
///
/// \param buf1 First buffer containing float values.
/// \param buf2 Second buffer containing float values.
/// \param callback Invoked for each position where buf1 > buf2.
void ScanMemCompareGreaterAvx2Float(std::span<const std::byte> buf1,
                                    std::span<const std::byte> buf2,
                                    std::function<void(size_t)> callback);

}  // namespace internal

/// \brief Scans a memory buffer for a byte pattern using SIMD acceleration.
///
/// Searches \p buffer for occurrences of \p pattern, reporting matches via
/// \p callback. Uses AVX2 when available for 32-byte parallel processing,
/// falling back to scalar comparison for small buffers or non-AVX2 systems.
///
/// \param buffer Source data to search. Must outlive the scan operation.
/// \param pattern Bytes to find. Empty patterns result in no matches.
/// \param alignment Only report matches at offsets divisible by this value.
///                  Common values: 1 (any), 2 (16-bit aligned), 4 (32-bit),
///                  8 (64-bit). Must be > 0.
/// \param callback Invoked for each match with the byte offset where the
///                 pattern starts. May be called 0 to N times.
///
/// \note The callback is invoked synchronously during the scan. Heavy
///       operations in the callback will directly impact scan performance.
///
/// Example - Finding a 4-byte pattern aligned to 4-byte boundaries:
/// \code
/// std::vector<std::byte> buffer = /* ... */;
/// std::array<std::byte, 4> pattern = {std::byte{0xDE}, std::byte{0xAD},
///                                     std::byte{0xBE}, std::byte{0xEF}};
/// ScanBuffer(buffer, pattern, 4, [](size_t offset) {
///   std::cout << "Found at offset: " << offset << "\n";
/// });
/// \endcode
template <ScanCallback Callback>
void ScanBuffer(std::span<const std::byte> buffer,
                std::span<const std::byte> pattern,
                size_t alignment,
                Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if (kHasAvx2) {
    internal::ScanBufferAvx2(
        buffer, pattern, alignment, std::forward<Callback>(callback));
  } else {
    internal::ScanBufferScalar(
        buffer, pattern, alignment, std::forward<Callback>(callback));
  }
}

namespace internal {

/// \brief AVX2 implementation for masked pattern scanning.
///
/// Processes 32 bytes at a time using AVX2 SIMD with mask support.
/// Falls back to scalar for patterns larger than 32 bytes.
/// Implemented in simd_scanner.cpp.
///
/// \param buffer Source data to search.
/// \param pattern Bytes to find.
/// \param mask Wildcard mask (0xFF = check byte, 0x00 = ignore).
/// \param callback Invoked for each match with byte offset.
void ScanBufferMaskedAvx2(std::span<const std::byte> buffer,
                          std::span<const std::byte> pattern,
                          std::span<const std::byte> mask,
                          std::function<void(size_t)> callback);

/// \brief Scalar fallback for masked pattern scanning.
///
/// Performs byte-by-byte comparison with mask support.
/// Each byte is ANDed with mask before comparison.
///
/// \param buffer Source data to search.
/// \param pattern Bytes to find.
/// \param mask Wildcard mask. Must be same size as pattern.
/// \param callback Invoked for each match with byte offset.
template <typename Callback>
void ScanBufferMaskedScalar(std::span<const std::byte> buffer,
                            std::span<const std::byte> pattern,
                            std::span<const std::byte> mask,
                            const Callback& callback) {
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
///
/// Searches \p buffer for occurrences of \p pattern using a wildcard \p mask,
/// reporting matches via \p callback. The mask allows certain bytes to be
/// ignored during comparison (useful for pointer scanning where some bytes
/// may vary).
///
/// \param buffer Source data to search.
/// \param pattern Bytes to find.
/// \param mask Wildcard mask. Must be same size as pattern.
///             0xFF = byte must match, 0x00 = byte is ignored (wildcard).
/// \param callback Invoked for each match with the byte offset.
///
/// \note Uses AVX2 when available and pattern fits in 32 bytes.
///       Falls back to scalar for larger patterns.
///
/// Example - Finding pattern with wildcard second byte:
/// \code
/// std::vector<std::byte> buffer = /* ... */;
/// std::vector<std::byte> pattern = {
///   std::byte{0xAA}, std::byte{0x00}, std::byte{0xCC}};
/// std::vector<std::byte> mask = {
///   std::byte{0xFF}, std::byte{0x00}, std::byte{0xFF}};
/// // Matches: 0xAA ?? 0xCC (second byte ignored)
/// ScanBufferMasked(buffer, pattern, mask, [](size_t offset) {
///   std::cout << "Found at offset: " << offset << "\n";
/// });
/// \endcode
template <ScanCallback Callback>
void ScanBufferMasked(std::span<const std::byte> buffer,
                      std::span<const std::byte> pattern,
                      std::span<const std::byte> mask,
                      Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if (kHasAvx2) {
    internal::ScanBufferMaskedAvx2(
        buffer, pattern, mask, std::forward<Callback>(callback));
  } else {
    internal::ScanBufferMaskedScalar(
        buffer, pattern, mask, std::forward<Callback>(callback));
  }
}

/// \brief Scans a memory buffer for a pattern (unaligned, byte-level).
///
/// \deprecated Prefer the aligned overload for type-safe scanning.
///             This overload uses alignment=1 which may be slower on
///             large datasets due to unaligned memory access patterns.
///
/// \param buffer Source data to search.
/// \param pattern Bytes to find.
/// \param callback Invoked for each match with byte offset.
template <ScanCallback Callback>
void ScanBuffer(std::span<const std::byte> buffer,
                std::span<const std::byte> pattern,
                Callback&& callback) {
  ScanBuffer(buffer, pattern, 1, std::forward<Callback>(callback));
}

/// \brief Scans two buffers for equality/inequality.
///
/// Compares two buffers position by position, reporting locations where
/// they match or differ based on \p find_equal parameter. Useful for
/// finding changed/unchanged values between memory snapshots.
///
/// \param buf1 First buffer to compare.
/// \param buf2 Second buffer to compare.
/// \param find_equal If true, report positions where buffers are identical.
///                   If false, report positions where buffers differ.
/// \param stride Number of bytes to compare at each position. Determines
///               the granularity of comparison (1=byte, 4=int32, 8=int64).
/// \param callback Invoked for each matching position with byte offset.
///
/// \note Uses AVX2 when available for 32-byte parallel comparison.
///       Falls back to scalar for small buffers.
///
/// Example - Finding all 4-byte values that changed:
/// \code
/// ScanMemCmp(old_snapshot, new_snapshot, false, 4, [](size_t offset) {
///   // offset is byte position where 4-byte values differ
///   int32_t old_val, new_val;
///   std::memcpy(&old_val, &old_snapshot[offset], 4);
///   std::memcpy(&new_val, &new_snapshot[offset], 4);
///   std::cout << "Value at " << offset << " changed: "
///             << old_val << " -> " << new_val << "\n";
/// });
/// \endcode
template <ScanCallback Callback>
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

/// \brief Scans for values where buf1[i] > buf2[i].
///
/// Interprets buffers as arrays of type T and reports positions where
/// values in buf1 are greater than corresponding values in buf2.
/// Supports int32_t and float with AVX2 acceleration; other types use
/// scalar fallback.
///
/// \tparam T Numeric type to interpret buffers as. Must be arithmetic.
///           Supported AVX2 types: int32_t, float.
/// \param buf1 First buffer containing values.
/// \param buf2 Second buffer containing values to compare against.
/// \param callback Invoked for each position where buf1 > buf2.
///                 Receives byte offset (not element index).
///                 To get element index: offset / sizeof(T).
///
/// \note AVX2 versions process 8 values (32 bytes) at a time.
///       Other types fall back to scalar comparison.
///
/// Example - Finding increased int32 values:
/// \code
/// ScanMemCompareGreater<int32_t>(old_values, new_values, [](size_t offset) {
///   // offset is in bytes
///   size_t index = offset / sizeof(int32_t);
///   int32_t old_val, new_val;
///   std::memcpy(&old_val, &old_values[offset], sizeof(int32_t));
///   std::memcpy(&new_val, &new_values[offset], sizeof(int32_t));
///   std::cout << "Value " << index << " increased: "
///             << old_val << " -> " << new_val << "\n";
/// });
/// \endcode
template <typename T, ScanCallback Callback>
void ScanMemCompareGreater(std::span<const std::byte> buf1,
                           std::span<const std::byte> buf2,
                           Callback&& callback) {
  static const bool kHasAvx2 = HasAvx2();
  if constexpr (std::is_same_v<T, int32_t>) {
    if (kHasAvx2) {
      internal::ScanMemCompareGreaterAvx2Int32(
          buf1, buf2, std::forward<Callback>(callback));
      return;
    }
  } else if constexpr (std::is_same_v<T, float>) {
    if (kHasAvx2) {
      internal::ScanMemCompareGreaterAvx2Float(
          buf1, buf2, std::forward<Callback>(callback));
      return;
    }
  }

  internal::ScanMemCompareGreaterScalar<T>(
      buf1, buf2, std::forward<Callback>(callback));
}

}  // namespace maia::core
