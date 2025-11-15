// Copyright (c) Maia

#include "maia/core/i_process.h"

namespace maia {

class SnapshotResultArena {};

class Snapshot {
 public:
  explicit Snapshot(IProcess& accessor);

  void UpdateFromPrevious();

  // TODO: This should return the memory, ScanResult, which is allocated via an
  // arena.
  void ScanChanged();

  struct SnapshotLayer {
    // Addresses from last scan.
    std::vector<uintptr_t> addresses;
    // Their values.
    std::vector<std::byte> values;
    // Size of each value.
    size_t value_size;
  };

  SnapshotLayer& get_snapshot() {
    return current_layer_;
  }

  SnapshotLayer& get_previous() {
    return previous_layer_;
  }

 private:
  IProcess& accessor_;

  SnapshotLayer previous_layer_;
  SnapshotLayer current_layer_;
};

}  // namespace maia
