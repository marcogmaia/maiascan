// Copyright (c) Maia

#pragma once

#include <cstdint>
#include <future>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_set>
#include <vector>

#include "maia/core/i_process.h"
#include "maia/core/pointer_map.h"
#include "maia/mmem/mmem.h"

namespace maia::core {

struct PointerPath {
  // The static base address (e.g., address within game.exe)
  uint64_t base_address;

  // Name of the module containing base_address (e.g., "game.exe")
  // Empty if address is not within a known module.
  std::string module_name;

  // Offset from module base to the pointer (base_address - module_base)
  uint64_t module_offset;

  // Chain of offsets to follow. The last offset leads to the target.
  // Example: [0x10, 0x48] means:
  //   [[base_address] + 0x10] + 0x48 = target
  std::vector<int64_t> offsets;
};

struct PointerScanConfig {
  // The target address we want to find paths to
  uint64_t target_address = 0;

  // Maximum depth of pointer chain (e.g., 7 means up to 7 dereferences)
  uint32_t max_level = 7;

  // Maximum offset at each level (e.g., 4096 bytes)
  // Only positive offsets are searched: [pointed_value, pointed_value +
  // max_offset]
  uint32_t max_offset = 4096;

  // If true, also search negative offsets: [pointed_value - max_offset,
  // pointed_value]
  bool allow_negative_offsets = false;

  // Maximum number of results to return (0 = unlimited)
  uint32_t max_results = 0;

  // Only accept paths ending in these modules (empty = accept all static
  // addresses)
  std::unordered_set<std::string> allowed_modules;

  // Known last offsets filter (simplified).
  // Index 0 = last offset (closest to target), index 1 = second-to-last, etc.
  // If non-empty, paths must end with this exact sequence of offsets.
  // Example: To filter paths ending in [..., 0x10, 0x58] (where 0x58 is last):
  //   last_offsets = {0x58, 0x10};
  std::vector<int64_t> last_offsets;
};

struct PointerScanResult {
  std::vector<PointerPath> paths;
  bool success = false;
  std::string error_message;
  uint64_t paths_evaluated = 0;  // For statistics
};

class PointerScanner {
 public:
  PointerScanner() = default;

  /// \brief Find all pointer paths from static addresses to the target.
  /// \param map The pre-generated pointer map.
  /// \param config Scan configuration (target, depth, etc.).
  /// \param modules List of loaded modules to identify static addresses.
  /// \param stop_token Token to cancel the operation.
  /// \param progress_callback Optional callback for progress updates.
  /// \return The discovered paths.
  [[nodiscard]] PointerScanResult FindPaths(
      const PointerMap& map,
      const PointerScanConfig& config,
      const std::vector<mmem::ModuleDescriptor>& modules,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr) const;

  /// \brief Async version of FindPaths.
  [[nodiscard]] std::future<PointerScanResult> FindPathsAsync(
      const PointerMap& map,
      const PointerScanConfig& config,
      const std::vector<mmem::ModuleDescriptor>& modules,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr) const;

  /// \brief Filter existing paths: keep only those that resolve to the expected
  /// target in the current process state.
  /// \param process The live process to check against.
  /// \param paths The list of paths to validate.
  /// \param expected_target The address the path should resolve to.
  /// \return A new list containing only the valid paths.
  [[nodiscard]] std::vector<PointerPath> FilterPaths(
      IProcess& process,
      const std::vector<PointerPath>& paths,
      uint64_t expected_target) const;

  /// \brief Resolve a single path in the current process.
  /// \param process The live process to read from.
  /// \param path The path to resolve.
  /// \return The final address, or nullopt if resolution fails (invalid
  /// pointer).
  [[nodiscard]] std::optional<uint64_t> ResolvePath(
      IProcess& process, const PointerPath& path) const;

  /// \brief Resolve a single path using cached modules.
  /// \param process The live process to read from.
  /// \param path The path to resolve.
  /// \param modules Cached list of modules to resolve static addresses.
  /// \return The final address, or nullopt if resolution fails.
  [[nodiscard]] std::optional<uint64_t> ResolvePath(
      IProcess& process,
      const PointerPath& path,
      const std::vector<mmem::ModuleDescriptor>& modules) const;
};

}  // namespace maia::core
