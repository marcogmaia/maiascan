// Copyright (c) Maia

#pragma once

#include "maia/core/i_memory_scanner.h"
#include "maia/core/i_process.h"

namespace maia {

class MemoryScanner : public IMemoryScanner {
 public:
  explicit MemoryScanner(IProcess& process)
      : process_(process),
        memory_regions_(process_.GetMemoryRegions()) {}

  ScanResult NewScan(const ScanParams& params) override;

  ScanResult NextScan(const ScanResult& previous_result,
                      const ScanParams& params) override;

 private:
  IProcess& process_;
  std::vector<MemoryRegion> memory_regions_;
  std::shared_ptr<const MemorySnapshot> snapshot_;
};

}  // namespace maia
