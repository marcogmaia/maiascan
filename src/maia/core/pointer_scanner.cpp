// Copyright (c) Maia

#include "maia/core/pointer_scanner.h"

#include <algorithm>
#include <deque>
#include <unordered_set>

#include "maia/logging.h"

namespace maia::core {

namespace {

struct SearchNode {
  uint64_t address;
  std::vector<int64_t> offsets;
  uint32_t level;
};

// Helper to check if an address is static (inside a module)
// Returns pointer to module if found, nullptr otherwise.
const mmem::ModuleDescriptor* FindModuleForAddress(
    uint64_t address, const std::vector<mmem::ModuleDescriptor>& modules) {
  for (const auto& mod : modules) {
    if (address >= mod.base && address < mod.end) {
      return &mod;
    }
  }
  return nullptr;
}

}  // namespace

PointerScanResult PointerScanner::FindPaths(
    const PointerMap& map,
    const PointerScanConfig& config,
    const std::vector<mmem::ModuleDescriptor>& modules,
    std::stop_token stop_token,
    ProgressCallback progress_callback) const {
  PointerScanResult result;
  result.success = true;  // Assume success unless error occurs

  // BFS Queue: (CurrentAddress, OffsetsSoFar, Level)
  std::deque<SearchNode> queue;
  queue.push_back(
      {.address = config.target_address, .offsets = {}, .level = 0});

  // Visited set to prevent loops and redundant work
  std::unordered_set<uint64_t> visited;
  visited.insert(config.target_address);

  uint64_t paths_evaluated = 0;

  // We process level by level to respect max_level.
  while (!queue.empty()) {
    if (stop_token.stop_requested()) {
      result.success = false;
      result.error_message = "Scan cancelled";
      break;
    }

    if (config.max_results > 0 && result.paths.size() >= config.max_results) {
      break;
    }

    SearchNode current = std::move(queue.front());
    queue.pop_front();

    // Check if we hit the level limit
    if (current.level >= config.max_level) {
      continue;
    }

    // Search range: [address - max_offset, address]
    // Or [address - max_offset, address + max_offset] if negative allowed.
    // The stored value in map is what points TO the current address.
    // *P = V. We want V approx CurrentAddress.
    // Offset = CurrentAddress - V.
    // If we only allow positive offsets: V <= CurrentAddress, so V in
    // [CurrentAddress - MaxOffset, CurrentAddress].

    uint64_t min_val = (current.address > config.max_offset)
                           ? current.address - config.max_offset
                           : 0;
    uint64_t max_val = current.address;

    if (config.allow_negative_offsets) {
      max_val = current.address + config.max_offset;
    }

    auto pointers = map.FindPointersToRange(min_val, max_val);

    for (const auto& entry : pointers) {
      if (stop_token.stop_requested()) {
        break;
      }

      paths_evaluated++;

      // Prevent loops
      if (visited.contains(entry.address)) {
        continue;
      }

      int64_t offset = static_cast<int64_t>(current.address) -
                       static_cast<int64_t>(entry.value);

      // Create new offset chain.
      // Because we trace backwards (Target <- ... <- Base), the offset we just
      // found (Current - Value) is the LAST offset applied in the forward path.
      // We push_back and reverse at the end of finding a static root.
      std::vector<int64_t> next_offsets = current.offsets;
      next_offsets.push_back(offset);

      // Check if this pointer is a Static Address
      const auto* mod = FindModuleForAddress(entry.address, modules);
      if (mod) {
        // Found a path! Reconstruct path.
        PointerPath path;
        path.base_address = entry.address;
        path.module_name = mod->name;
        path.module_offset = entry.address - mod->base;
        path.offsets = next_offsets;
        std::reverse(path.offsets.begin(), path.offsets.end());

        // Check module filter
        bool allowed = true;
        if (!config.allowed_modules.empty()) {
          bool found = false;
          for (const auto& allowed_name : config.allowed_modules) {
            if (mod->name == allowed_name) {
              found = true;
              break;
            }
          }
          if (!found) {
            allowed = false;
          }
        }

        if (allowed) {
          result.paths.push_back(std::move(path));
        }

        // Static addresses are considered roots; stop searching this branch.
        continue;
      }

      // Not static, continue search if level permits
      visited.insert(entry.address);
      queue.push_back(
          {entry.address, std::move(next_offsets), current.level + 1});
    }
  }

  result.paths_evaluated = paths_evaluated;
  return result;
}

std::future<PointerScanResult> PointerScanner::FindPathsAsync(
    const PointerMap& map,
    const PointerScanConfig& config,
    const std::vector<mmem::ModuleDescriptor>& modules,
    std::stop_token stop_token,
    ProgressCallback progress_callback) const {
  return std::async(
      std::launch::async,
      [this, &map, config, modules, stop_token, progress_callback]() {
        return FindPaths(map, config, modules, stop_token, progress_callback);
      });
}

std::optional<uint64_t> PointerScanner::ResolvePath(
    IProcess& process, const PointerPath& path) const {
  uint64_t current_base = path.base_address;

  if (!path.module_name.empty()) {
    // Find module in current process.
    auto modules = process.GetModules();
    bool found = false;
    for (const auto& mod : modules) {
      if (mod.name == path.module_name) {
        current_base = mod.base + path.module_offset;
        found = true;
        break;
      }
    }
    if (!found) {
      return std::nullopt;  // Module not loaded
    }
  }

  uint64_t current_addr = current_base;

  // Follow the chain: Val = Read(Current), Current = Val + Offset.
  for (size_t i = 0; i < path.offsets.size(); ++i) {
    // TODO: Add GetPointerSize() to IProcess. Assuming 8 bytes for now.
    uint64_t ptr_val = 0;
    std::byte buffer[8];
    size_t ptr_size = 8;
    if (!process.ReadMemory(
            std::span<const uintptr_t>{&current_addr, 1}, ptr_size, buffer)) {
      return std::nullopt;
    }

    ptr_val = *reinterpret_cast<uint64_t*>(buffer);
    current_addr = ptr_val + path.offsets[i];
  }

  return current_addr;
}

std::vector<PointerPath> PointerScanner::FilterPaths(
    IProcess& process,
    const std::vector<PointerPath>& paths,
    uint64_t expected_target) const {
  std::vector<PointerPath> valid_paths;
  valid_paths.reserve(paths.size());

  for (const auto& path : paths) {
    auto resolved = ResolvePath(process, path);
    if (resolved.has_value() && *resolved == expected_target) {
      valid_paths.push_back(path);
    }
  }

  return valid_paths;
}

}  // namespace maia::core
