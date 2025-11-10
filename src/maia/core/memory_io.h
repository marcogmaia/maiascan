// Copyright (c) Maia

#pragma once

#include "maia/core/i_process.h"
#include "maia/core/scan_types.h"

namespace maia {

template <CScannableType T>
T ReadAt(IProcess& process, MemoryAddress address) {
  T value;
  process.ReadMemory(address, ToBytesView(value));
  return value;
}

template <CScannableType T>
bool WriteAt(IProcess& process, MemoryAddress address, const T& value) {
  return process.WriteMemory(address, ToBytesView(value));
}

}  // namespace maia
