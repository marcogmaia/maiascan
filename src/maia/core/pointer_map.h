// Copyright (c) Maia

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

#include "maia/core/i_process.h"

namespace maia::core {

// Progress callback signature.
// Param progress: Value between 0.0 and 1.0 indicating completion percentage.
using ProgressCallback = std::function<void(float progress)>;

struct PointerMapEntry {
  uint64_t address;  // Where the pointer lives
  uint64_t value;    // What it points to

  // Allow sorting by value for efficient range queries
  bool operator<(const PointerMapEntry& other) const {
    return value < other.value;
  }
};

static_assert(sizeof(PointerMapEntry) == 16);

/// \brief A searchable snapshot of all pointers in a process.
/// \details The PointerMap stores pairs of (Address, Value) for every
/// pointer-sized integer in the process memory that points to a valid
/// memory region. It is designed for efficient reverse lookups ("what points
/// to X?") using binary search.
class PointerMap {
 public:
  /// \brief Factory method: Generate a pointer map from a live process.
  /// \param process The target process to snapshot.
  /// \param stop_token Token to cancel the operation.
  /// \param progress_callback Optional callback for progress updates.
  /// \return A PointerMap instance or nullopt if cancelled/failed.
  static std::optional<PointerMap> Generate(
      IProcess& process,
      std::stop_token stop_token = {},
      ProgressCallback progress_callback = nullptr);

  /// \brief Factory method: Load a pointer map from a stream.
  /// \param stream The input stream to read from.
  /// \return A PointerMap instance or nullopt if loading failed.
  static std::optional<PointerMap> Load(std::istream& stream);

  /// \brief Factory method: Load a pointer map from disk.
  /// \param path File path to the .pmap file.
  /// \return A PointerMap instance or nullopt if loading failed.
  static std::optional<PointerMap> Load(const std::filesystem::path& path);

  PointerMap() = default;

  // Move-only (expensive to copy due to large vector)
  PointerMap(PointerMap&&) = default;
  PointerMap& operator=(PointerMap&&) = default;
  PointerMap(const PointerMap&) = delete;
  PointerMap& operator=(const PointerMap&) = delete;

  /// \brief Save the pointer map to a stream.
  /// \param stream The output stream to write to.
  /// \return true if successful.
  bool Save(std::ostream& stream) const;

  /// \brief Save the pointer map to disk.
  /// \param path File path for the .pmap file.
  /// \return true if successful.
  bool Save(const std::filesystem::path& path) const;

  /// \brief Find all entries where value is in [min_value, max_value].
  /// \details Uses binary search to find pointers pointing to the range.
  /// \return A span of entries sorted by value.
  [[nodiscard]] std::span<const PointerMapEntry> FindPointersToRange(
      uint64_t min_value, uint64_t max_value) const;

  /// \brief Get the pointer size used during generation (4 or 8 bytes).
  [[nodiscard]] size_t GetPointerSize() const {
    return pointer_size_;
  }

  /// \brief Get total number of entries.
  [[nodiscard]] size_t GetEntryCount() const {
    return entries_.size();
  }

  /// \brief Get the process name this map was generated from.
  [[nodiscard]] const std::string& GetProcessName() const {
    return process_name_;
  }

  /// \brief Get the timestamp when this map was generated.
  [[nodiscard]] uint64_t GetTimestamp() const {
    return timestamp_;
  }

 private:
  std::vector<PointerMapEntry> entries_;  // Sorted by value
  size_t pointer_size_ = 8;
  std::string process_name_;
  uint64_t timestamp_ = 0;
};

}  // namespace maia::core
