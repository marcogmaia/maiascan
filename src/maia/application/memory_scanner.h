// Copyright (c) Maia

#pragma once

#include "maia/core/i_memory_scanner.h"
#include "maia/core/i_process.h"
#include "maia/core/scan_result.h"
#include "maia/logging.h"

namespace maia {

class MemoryScanner {
 public:
  explicit MemoryScanner(std::unique_ptr<IProcess> process)
      : process_(std::move(process)),
        memory_regions_(process_->GetMemoryRegions()) {}

  // ScanResult NewScan(const ScanParams& params);

  // ScanResult NextScan(const ScanResult& previous_result,
  //                     const ScanParams& params);

  // void UpdateFromPrevious(IProcess& accessor) {
  //   // This is where we beat CheatEngine!
  //   // Instead of scanning all memory, only check addresses from previous
  //   scan

  //   std::vector<std::byte> new_values(current_layer_.addresses.size() *
  //                                     current_layer_.value_size);

  //   // Use batch ReadMemory (Component 3) - single syscall on Linux
  //   accessor.ReadMemory(std::span(current_layer_.addresses),
  //                       current_layer_.value_size,
  //                       std::span(new_values));

  //   // Compare old vs new values and update current_layer_
  //   for (size_t i = 0; i < current_layer_.addresses.size(); ++i) {
  //     size_t offset = i * current_layer_.value_size;
  //     if (memcmp(&new_values[offset],
  //                &previous_layer_.values[offset],
  //                current_layer_.value_size) != 0) {
  //       // Value changed - update current layer
  //       memcpy(&current_layer_.values[offset],
  //              &new_values[offset],
  //              current_layer_.value_size);
  //     }
  //   }
  // }

 private:
  std::unique_ptr<IProcess> process_;
  std::vector<MemoryRegion> memory_regions_;
  std::shared_ptr<const MemorySnapshot> snapshot_;
};

}  // namespace maia
