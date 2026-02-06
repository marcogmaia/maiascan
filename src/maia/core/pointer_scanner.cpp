// Copyright (c) Maia

#include "maia/core/pointer_scanner.h"

#include <algorithm>
#include <deque>
#include <ranges>
#include <unordered_set>

namespace maia::core {

namespace {

struct SearchNode {
  uint64_t address;
  std::vector<int64_t> offsets;
  uint32_t level;
};

// Helper to check if an address is static (inside a module)
// Returns module descriptor if found, nullopt otherwise.
std::optional<mmem::ModuleDescriptor> FindModuleForAddress(
    uint64_t address, const std::vector<mmem::ModuleDescriptor>& modules) {
  auto it = std::ranges::find_if(modules, [address](const auto& mod) {
    return address >= mod.base && address < mod.end;
  });
  return (it != modules.end()) ? std::make_optional(*it) : std::nullopt;
}

// Helper to find a module by name.
std::optional<mmem::ModuleDescriptor> FindModuleByName(
    const std::string_view name,
    const std::vector<mmem::ModuleDescriptor>& modules) {
  auto it = std::ranges::find_if(
      modules, [name](const auto& mod) { return mod.name == name; });
  return (it != modules.end()) ? std::make_optional(*it) : std::nullopt;
}

std::optional<uint64_t> FollowPointerChain(
    IProcess& process,
    uint64_t start_address,
    const std::vector<int64_t>& offsets) {
  uint64_t current_addr = start_address;
  const size_t ptr_size = process.GetPointerSize();

  for (const int64_t offset : offsets) {
    uint64_t ptr_val = 0;
    // Read the pointer value at current_addr
    if (!process.ReadMemory(std::span<const uintptr_t>{&current_addr, 1},
                            ptr_size,
                            std::as_writable_bytes(std::span{&ptr_val, 1}),
                            nullptr)) {
      return std::nullopt;
    }
    // Mask to ensure only the valid pointer bytes are used.
    // This handles 32-bit pointers correctly regardless of endianness.
    if (ptr_size == 4) {
      ptr_val &= 0xFFFFFFFFULL;
    }
    // Apply offset to get the next address in the chain
    current_addr = ptr_val + offset;
  }

  return current_addr;
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
  uint32_t last_reported_level = 0xFFFFFFFF;

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

    // Report progress when level changes.
    if (progress_callback && current.level != last_reported_level) {
      progress_callback(static_cast<float>(current.level) / config.max_level);
      last_reported_level = current.level;
    }

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

      ++paths_evaluated;

      // Prevent loops
      if (visited.contains(entry.address)) {
        continue;
      }

      int64_t offset = static_cast<int64_t>(current.address) -
                       static_cast<int64_t>(entry.value);

      // Check last_offsets filter: if this level has a constraint, verify
      // the offset matches the expected value. Wildcards (nullopt) match any.
      if (current.level < config.last_offsets.size()) {
        const auto& expected = config.last_offsets[current.level];
        if (expected.has_value() && offset != *expected) {
          continue;
        }
      }

      // Create new offset chain.
      // Because we trace backwards (Target <- ... <- Base), the offset we just
      // found (Current - Value) is the LAST offset applied in the forward path.
      // We push_back and reverse at the end of finding a static root.
      std::vector<int64_t> next_offsets = current.offsets;
      next_offsets.push_back(offset);

      // Check if this pointer is a Static Address
      const auto mod = FindModuleForAddress(entry.address, modules);

      if (!mod) {
        // Not static, continue search if level permits
        visited.insert(entry.address);
        queue.emplace_back(SearchNode{.address = entry.address,
                                      .offsets = std::move(next_offsets),
                                      .level = current.level + 1});
        continue;
      }

      // Found a static root!
      // Check max_results limit before adding
      if (config.max_results > 0 && result.paths.size() >= config.max_results) {
        continue;
      }

      // Check module filter
      if (!config.allowed_modules.empty() &&
          !config.allowed_modules.contains(mod->name)) {
        continue;
      }

      // Found a path! Reconstruct path.
      PointerPath path;
      path.base_address = entry.address;
      path.module_name = mod->name;
      path.module_offset = entry.address - mod->base;
      path.offsets = std::move(next_offsets);
      std::reverse(path.offsets.begin(), path.offsets.end());

      result.paths.push_back(std::move(path));
    }
  }

  if (progress_callback && result.success) {
    progress_callback(1.0f);
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
  return ResolvePath(process, path, process.GetModules());
}

std::optional<uint64_t> PointerScanner::ResolvePath(
    IProcess& process,
    const PointerPath& path,
    const std::vector<mmem::ModuleDescriptor>& modules) const {
  uint64_t current_base = path.base_address;

  if (!path.module_name.empty()) {
    const auto mod = FindModuleByName(path.module_name, modules);

    if (!mod) {
      return std::nullopt;
    }
    current_base = mod->base + path.module_offset;
  }

  uint64_t current_addr = current_base;

  return FollowPointerChain(process, current_addr, path.offsets);
}

std::vector<PointerPath> PointerScanner::FilterPaths(
    IProcess& process,
    const std::vector<PointerPath>& paths,
    uint64_t expected_target) const {
  std::vector<PointerPath> valid_paths;
  valid_paths.reserve(paths.size());

  const auto is_valid = [&](const auto& path) {
    auto resolved = ResolvePath(process, path);
    return resolved.has_value() && *resolved == expected_target;
  };

  std::ranges::copy(paths | std::views::filter(is_valid),
                    std::back_inserter(valid_paths));

  return valid_paths;
}

}  // namespace maia::core
