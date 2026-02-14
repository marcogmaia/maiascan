// Copyright (c) Maia

#include "maia/core/pointer_map.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <vector>

#include "maia/logging.h"
#include "maia/mmem/mmem.h"

namespace maia::core {

namespace {

constexpr size_t kChunkSize =
    static_cast<const size_t>(64 * 1024 * 1024);  // 64 MB chunks

// File header structure (must match design doc)
struct FileHeader {
  char magic[8] = {'M', 'A', 'I', 'A', 'P', 'T', 'R', '\0'};
  uint32_t version = 1;
  uint32_t pointer_size = 0;
  uint64_t entry_count = 0;
  uint64_t timestamp = 0;
  uint32_t flags = 0;
  uint32_t process_name_len = 0;
  uint8_t reserved[24] = {0};
};

// Helper to check if a value points to valid memory
bool IsValidPointer(uint64_t ptr_val,
                    const std::vector<MemoryRegion>& sorted_regions) {
  // Use ranges::upper_bound to find the first region with base > ptr_val.
  // We project the MemoryRegion to its 'base' member for comparison.
  // std::less{} is the default comparator, but explicitly passing it allows us
  // to pass the projection.
  auto it = std::ranges::upper_bound(
      sorted_regions, ptr_val, std::less{}, &MemoryRegion::base);

  if (it == sorted_regions.begin()) {
    return false;
  }

  const auto& candidate = *std::prev(it);
  return ptr_val < (candidate.base + candidate.size);
}

}  // namespace

std::optional<PointerMap> PointerMap::Generate(
    IProcess& process,
    std::stop_token stop_token,
    ProgressCallback progress_callback) {
  PointerMap map;
  map.process_name_ = process.GetProcessName();
  map.timestamp_ = std::chrono::system_clock::now().time_since_epoch().count();
  map.pointer_size_ = process.GetPointerSize();

  auto regions = process.GetMemoryRegions();

  // Pre-sort regions for efficient validation
  std::ranges::sort(regions, [](const MemoryRegion& a, const MemoryRegion& b) {
    return a.base < b.base;
  });

  // Calculate total size for progress reporting
  size_t total_bytes = 0;
  for (const auto& region : regions) {
    if (region.protection != mmem::Protection::kNone) {
      total_bytes += region.size;
    }
  }

  // Heuristic: reserve 5% of total bytes as pointers to reduce reallocations
  map.entries_.reserve((total_bytes / map.pointer_size_) / 20);

  size_t processed_bytes = 0;
  std::vector<std::byte> buffer;
  buffer.reserve(kChunkSize);

  // We scan all readable regions
  for (const auto& region : regions) {
    if (stop_token.stop_requested()) {
      return std::nullopt;
    }

    // Skip non-readable regions
    if ((static_cast<uint32_t>(region.protection) &
         static_cast<uint32_t>(mmem::Protection::kRead)) == 0) {
      continue;
    }

    // Process region in chunks
    for (size_t offset = 0; offset < region.size; offset += kChunkSize) {
      if (stop_token.stop_requested()) {
        return std::nullopt;
      }

      size_t read_size = std::min(kChunkSize, region.size - offset);
      buffer.resize(read_size);

      // Read memory
      std::vector<uintptr_t> addr_vec = {region.base + offset};
      if (!process.ReadMemory(addr_vec, read_size, buffer, nullptr)) {
        // Failed to read chunk, skip it
        processed_bytes += read_size;
        if (progress_callback) {
          progress_callback(static_cast<float>(processed_bytes) / total_bytes);
        }
        continue;
      }

      // Scan buffer for pointers
      const size_t step = map.pointer_size_;
      const size_t limit = buffer.size() - step;

      for (size_t i = 0; i <= limit; i += step) {
        uint64_t ptr_val = 0;
        if (map.pointer_size_ == 8) {
          ptr_val = *reinterpret_cast<const uint64_t*>(&buffer[i]);
        } else {
          ptr_val = *reinterpret_cast<const uint32_t*>(&buffer[i]);
        }

        if (IsValidPointer(ptr_val, regions)) {
          map.entries_.push_back(
              {.address = region.base + offset + i, .value = ptr_val});
        }
      }

      processed_bytes += read_size;
      if (progress_callback) {
        progress_callback(static_cast<float>(processed_bytes) / total_bytes);
      }
    }
  }

  // TODO(marco): Maybe we should encapsulate this in a class to enforce this
  // binary-searchable invariance.
  // Sort by value to enable binary search.
  std::ranges::sort(map.entries_, std::less{});

  return map;
}

std::optional<PointerMap> PointerMap::Load(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    maia::LogError("Failed to open pointer map file: {}", path.string());
    return std::nullopt;
  }
  return Load(file);
}

std::optional<PointerMap> PointerMap::Load(std::istream& stream) {
  // Get stream size if possible
  stream.seekg(0, std::ios::end);
  std::fpos total_size = stream.tellg();
  stream.seekg(0, std::ios::beg);

  FileHeader header;
  if (!stream.read(reinterpret_cast<char*>(&header), sizeof(header))) {
    return std::nullopt;
  }

  // Verify magic
  if (std::memcmp(header.magic, "MAIAPTR\0", 8) != 0) {
    maia::LogError("Invalid magic bytes in pointer map");
    return std::nullopt;
  }

  // Basic size validation to prevent memory exhaustion
  if (total_size != -1) {
    uint64_t expected_min_size = sizeof(FileHeader) + header.process_name_len +
                                 (header.entry_count * sizeof(PointerMapEntry));
    // NOLINTNEXTLINE
    if (static_cast<uint64_t>(total_size) < expected_min_size) {
      maia::LogError("Pointer map file too small for declared entry count");
      return std::nullopt;
    }
  }

  // Limit entry count to something reasonable (e.g. 1 billion entries = ~16GB)
  if (header.entry_count > 1000000000) {
    maia::LogError("Pointer map entry count too large: {}", header.entry_count);
    return std::nullopt;
  }

  PointerMap map;
  map.pointer_size_ = header.pointer_size;
  map.timestamp_ = header.timestamp;
  map.process_name_.resize(header.process_name_len);

  if (header.process_name_len > 0) {
    if (!stream.read(map.process_name_.data(), header.process_name_len)) {
      return std::nullopt;
    }
  }

  // Read padding
  size_t current_pos = sizeof(FileHeader) + header.process_name_len;
  size_t padding = (8 - (current_pos % 8)) % 8;
  stream.seekg(padding, std::ios::cur);

  // Read entries
  try {
    map.entries_.resize(header.entry_count);
    if (!stream.read(reinterpret_cast<char*>(map.entries_.data()),
                     header.entry_count * sizeof(PointerMapEntry))) {
      maia::LogError("Failed to read pointer map entries (unexpected EOF)");
      return std::nullopt;
    }
  } catch (const std::bad_alloc&) {
    maia::LogError("Failed to allocate memory for {} pointer map entries",
                   header.entry_count);
    return std::nullopt;
  }

  return map;
}

bool PointerMap::Save(const std::filesystem::path& path) const {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  return Save(file);
}

bool PointerMap::Save(std::ostream& stream) const {
  FileHeader header;
  header.pointer_size = static_cast<uint32_t>(pointer_size_);
  header.entry_count = entries_.size();
  header.timestamp = timestamp_;
  header.process_name_len = static_cast<uint32_t>(process_name_.size());

  stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
  stream.write(process_name_.data(), process_name_.size());

  // Padding
  size_t current_pos = sizeof(FileHeader) + process_name_.size();
  size_t padding = (8 - (current_pos % 8)) % 8;
  const char zeros[8] = {0};
  stream.write(zeros, padding);

  // Entries
  stream.write(reinterpret_cast<const char*>(entries_.data()),
               entries_.size() * sizeof(PointerMapEntry));

  return stream.good();
}

std::span<const PointerMapEntry> PointerMap::FindPointersToRange(
    uint64_t min_value, uint64_t max_value) const {
  // Projection: "&PointerMapEntry::value" tells the algorithm to look at that
  // member.
  const auto it_begin = std::ranges::lower_bound(
      entries_, min_value, {}, &PointerMapEntry::value);

  // Optimization: Start the second search from 'it_begin' (narrower range).
  const auto it_end = std::ranges::upper_bound(
      it_begin, entries_.end(), max_value, {}, &PointerMapEntry::value);

  return {it_begin, it_end};
}

}  // namespace maia::core
