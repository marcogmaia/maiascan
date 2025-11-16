// Copyright (c) Maia

#include "maia/core/process.h"

#include <optional>

#include "maia/core/memory_common.h"
#include "maia/mmem/mmem.h"

namespace maia {

namespace {

struct IndexedAddress {
  MemoryAddress address;
  size_t original_index;
};

struct BatchRange {
  size_t start_index = 0;
  size_t count = 0;
  MemoryAddress start_addr = 0;
  MemoryAddress end_addr = 0;
};

std::vector<IndexedAddress> CreateIndexedAddresses(
    std::span<const MemoryAddress> addresses) {
  std::vector<IndexedAddress> indexed;
  indexed.reserve(addresses.size());
  for (size_t i = 0; i < addresses.size(); ++i) {
    indexed.emplace_back(addresses[i], i);
  }
  return indexed;
}

void SortIndexedAddresses(std::vector<IndexedAddress>& indexed_addresses) {
  std::ranges::sort(indexed_addresses, {}, &IndexedAddress::address);
}

BatchRange FindNextBatch(const std::vector<IndexedAddress>& indexed_addresses,
                         size_t start_index,
                         size_t bytes_per_address,
                         size_t max_batch_size,
                         size_t max_gap_bytes) {
  const size_t n = indexed_addresses.size();
  if (start_index >= n) {
    return {};
  }

  const MemoryAddress base_addr = indexed_addresses[start_index].address;
  MemoryAddress batch_start = base_addr;
  MemoryAddress batch_end = base_addr + bytes_per_address;
  size_t count = 1;

  for (size_t i = start_index + 1; i < n; ++i) {
    const MemoryAddress next_addr = indexed_addresses[i].address;
    const MemoryAddress next_end = next_addr + bytes_per_address;

    const bool no_gap = next_addr <= batch_end;
    const bool small_gap = !no_gap && (next_addr - batch_end <= max_gap_bytes);
    const bool within_size = (next_end - batch_start) <= max_batch_size;

    if (!within_size || (!no_gap && !small_gap)) {
      break;
    }

    batch_end = std::max(batch_end, next_end);
    count++;
  }

  return BatchRange{.start_index = start_index,
                    .count = count,
                    .start_addr = batch_start,
                    .end_addr = batch_end};
}

bool TryReadBatch(mmem::ProcessDescriptor descriptor,
                  const BatchRange& batch,
                  std::vector<std::byte>& batch_buffer) {
  const size_t region_size = batch.end_addr - batch.start_addr;
  batch_buffer.resize(region_size);
  const size_t bytes_read =
      mmem::ReadMemory(descriptor, batch.start_addr, batch_buffer);
  return bytes_read == region_size;
}

void ExtractFromBatch(const std::vector<IndexedAddress>& indexed_addresses,
                      const BatchRange& batch,
                      const std::vector<std::byte>& batch_buffer,
                      size_t bytes_per_address,
                      std::span<std::byte> out_buffer) {
  for (size_t i = 0; i < batch.count; ++i) {
    const auto& item = indexed_addresses[batch.start_index + i];
    const size_t offset = item.address - batch.start_addr;

    auto dest = out_buffer.subspan(item.original_index * bytes_per_address,
                                   bytes_per_address);

    std::copy_n(&batch_buffer[offset], bytes_per_address, dest.begin());
  }
}

bool ReadIndividualAddresses(
    mmem::ProcessDescriptor descriptor,
    const std::vector<IndexedAddress>& indexed_addresses,
    const BatchRange& batch,
    size_t bytes_per_address,
    std::span<std::byte> out_buffer) {
  bool all_succeeded = true;

  for (size_t i = 0; i < batch.count; ++i) {
    const auto& item = indexed_addresses[batch.start_index + i];
    auto dest = out_buffer.subspan(item.original_index * bytes_per_address,
                                   bytes_per_address);

    const size_t bytes_read = mmem::ReadMemory(descriptor, item.address, dest);
    if (bytes_read != bytes_per_address) {
      all_succeeded = false;
    }
  }

  return all_succeeded;
}

}  // namespace

Process::Process(mmem::ProcessDescriptor descriptor) noexcept
    : descriptor_(std::move(descriptor)) {}

std::optional<Process> Process::Create(uint32_t pid) {
  std::optional<mmem::ProcessDescriptor> descriptor = mmem::GetProcess(pid);
  if (!descriptor) {
    return std::nullopt;
  }
  return Process(std::move(*descriptor));
}

std::optional<Process> Process::Create(std::string_view name) {
  std::optional<mmem::ProcessDescriptor> descriptor = mmem::FindProcess(name);
  if (!descriptor) {
    return std::nullopt;
  }
  return Process(std::move(*descriptor));
}

bool Process::ReadMemory(std::span<const MemoryAddress> addresses,
                         size_t bytes_per_address,
                         std::span<std::byte> out_buffer) {
  if (out_buffer.size() < addresses.size() * bytes_per_address) {
    return false;
  }

  if (addresses.empty()) {
    return true;
  }

  constexpr size_t kMaxBatchSize = 64 * 1024;
  constexpr size_t kMaxGapBytes = 256;

  auto indexed_addresses = CreateIndexedAddresses(addresses);
  SortIndexedAddresses(indexed_addresses);

  bool all_succeeded = true;
  std::vector<std::byte> batch_buffer;

  size_t current_index = 0;
  while (current_index < indexed_addresses.size()) {
    const BatchRange batch = FindNextBatch(indexed_addresses,
                                           current_index,
                                           bytes_per_address,
                                           kMaxBatchSize,
                                           kMaxGapBytes);

    if (batch.count == 0) {
      break;
    }

    if (TryReadBatch(descriptor_, batch, batch_buffer)) {
      ExtractFromBatch(indexed_addresses,
                       batch,
                       batch_buffer,
                       bytes_per_address,
                       out_buffer);
    } else {
      const bool batch_success = ReadIndividualAddresses(
          descriptor_, indexed_addresses, batch, bytes_per_address, out_buffer);
      if (!batch_success) {
        all_succeeded = false;
      }
    }

    current_index += batch.count;
  }

  return all_succeeded;
}

bool Process::WriteMemory(uintptr_t address,
                          std::span<const std::byte> buffer) {
  // The mmem API returns bytes written; the interface expects bool.
  // Success is defined as writing the entire buffer.
  size_t bytes_written = mmem::WriteMemory(descriptor_, address, buffer);
  return bytes_written == buffer.size();
}

std::vector<MemoryRegion> Process::GetMemoryRegions() const {
  std::vector<MemoryRegion> regions;
  regions.reserve(16);

  mmem::EnumSegments(descriptor_,
                     [&regions](const mmem::SegmentDescriptor& segment) {
                       regions.emplace_back(segment);
                       return true;  // Continue enumeration
                     });

  return regions;
}

uint32_t Process::GetProcessId() const {
  return descriptor_.pid;
}

std::string Process::GetProcessName() const {
  return descriptor_.name;
}

bool Process::IsProcessValid() const {
  return mmem::IsProcessAlive(descriptor_);
}

uintptr_t Process::GetBaseAddress() const {
  // The process's base address is the base address of its main module.
  // We find the main module by looking for the module with the same
  // name as the process descriptor.
  std::optional<mmem::ModuleDescriptor> main_module =
      mmem::FindModule(descriptor_, descriptor_.name);

  if (main_module) {
    return main_module->base;
  }

  return 0;
}

}  // namespace maia
