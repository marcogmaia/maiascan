// Copyright (c) Maia

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace maia {

/// \brief Caches values with time-based throttling to limit memory read
/// frequency.
/// \details Returns cached values if they were fetched within the throttle
/// duration.
///          On cache miss or expiry, calls the provided fetch function to get a
///          fresh value.
class ThrottledValueCache {
 public:
  using TimePoint = std::chrono::steady_clock::time_point;
  using Duration = std::chrono::steady_clock::duration;
  using FetchFn =
      std::function<std::optional<std::vector<std::byte>>(uint64_t)>;

  explicit ThrottledValueCache(
      Duration duration = std::chrono::milliseconds(100));

  /// \brief Get a value, using cache if valid or fetching if expired/missing.
  /// \param address The address to read (used as cache key).
  /// \param fetch_fn Function to call to fetch the value on cache miss.
  /// \param now Current time point (injected for testability).
  /// \return The cached or freshly fetched value, or nullopt on fetch failure.
  [[nodiscard]] std::optional<std::vector<std::byte>> Get(
      uint64_t address,
      const FetchFn& fetch_fn,
      TimePoint now = std::chrono::steady_clock::now());

  /// \brief Clear all cached values.
  void Clear();

  /// \brief Get the number of cached entries (for testing).
  [[nodiscard]] size_t Size() const;

 private:
  struct Entry {
    std::vector<std::byte> data;
    TimePoint timestamp;
  };

  Duration duration_;
  std::unordered_map<uint64_t, Entry> cache_;
};

}  // namespace maia
