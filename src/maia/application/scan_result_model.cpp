// Copyright (c) Maia

#include "maia/application/scan_result_model.h"

#include <algorithm>
#include <ranges>

#include "maia/mmem/mmem.h"

namespace maia {

void ScanResultModel::FirstScan(std::vector<std::byte> value_to_scan) {
  std::vector<MemoryRegion> regions = process_->GetMemoryRegions();
  auto total_size = std::ranges::fold_left(
      regions | std::views::transform(&MemoryRegion::size), 0, std::plus<>());

  std::vector<std::byte> bytes;
  bytes.reserve(total_size);
  for (const auto& region : regions) {
    // std::vector<class Ty>

    // process_->ReadMemory();
  }

  // bytes_per_address, std::span<std::byte> out_buffer)
}

void ScanResultModel::SetActiveProcess(std::unique_ptr<IProcess> process) {
  process_ = std::move(process);
}

void ScanResultModel::Clear() {
  entries_.clear();
  prev_entries_.clear();
}

}  // namespace maia
