// Copyright (c) Maia

#include "maia/core/cpu_info.h"

#include <array>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

namespace maia::core {

namespace {

// Wrapper for CPUID instruction.
// info: Output array of 4 integers (EAX, EBX, ECX, EDX).
// function_id: The leaf to retrieve (EAX input).
#ifdef _MSC_VER
// Ref: https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex
void Cpuid(int info[4], int function_id) {
  __cpuid(info, function_id);
}

// Wrapper for CPUIDEX instruction.
// subfunction_id: The subleaf to retrieve (ECX input).
// Ref: https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex
void CpuidEx(int info[4], int function_id, int subfunction_id) {
  __cpuidex(info, function_id, subfunction_id);
}
#elif defined(__GNUC__) || defined(__clang__)
// <cpuid.h> provides macros that wrap the cpuid instruction using inline
// assembly. Ref:
// https://github.com/gcc-mirror/gcc/blob/master/gcc/config/i386/cpuid.h
void Cpuid(int info[4], int function_id) {
  __cpuid(function_id, info[0], info[1], info[2], info[3]);
}

void CpuidEx(int info[4], int function_id, int subfunction_id) {
  __cpuid_count(
      function_id, subfunction_id, info[0], info[1], info[2], info[3]);
}
#endif

}  // namespace

bool HasAvx2() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
  // info[0] = EAX, info[1] = EBX, info[2] = ECX, info[3] = EDX
  std::array<int, 4> cpu_info{};

  // Call CPUID with EAX=0: Get vendor ID and max supported function ID.
  Cpuid(cpu_info.data(), 0);
  int num_ids = cpu_info[0];

  // Need at least function ID 7 for extended feature flags (AVX2).
  if (num_ids < 7) {
    return false;
  }

  // Call CPUID with EAX=7, ECX=0: Get extended feature flags.
  // AVX2 support is indicated by bit 5 of EBX (info[1]).
  // Ref: Intel SDM Vol 2A, Table 3-8 "Information Returned by CPUID
  // Instruction"
  // https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
  CpuidEx(cpu_info.data(), 7, 0);
  return (cpu_info[1] & (1 << 5)) != 0;
#else
  // Non-x86 architectures (e.g. ARM) do not support AVX2.
  return false;
#endif
}

}  // namespace maia::core
