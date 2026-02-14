// Copyright (c) Maia

#include "maia/application/throttled_value_cache.h"

namespace maia {

ThrottledValueCache::ThrottledValueCache(Duration duration)
    : duration_(duration) {}

std::optional<std::vector<std::byte>> ThrottledValueCache::Get(
    uint64_t address, const FetchFn& fetch_fn, TimePoint now) {
  // Check cache first
  auto it = cache_.find(address);
  if (it != cache_.end() && (now - it->second.timestamp) < duration_) {
    return it->second.data;  // Return cached value
  }

  // Cache miss or expired - fetch new value
  auto value = fetch_fn(address);
  if (value) {
    cache_[address] = {.data = *value,
                       .timestamp = now};  // Store copy in cache
  }
  return value;  // Return the fetched value
}

void ThrottledValueCache::Clear() {
  cache_.clear();
}

size_t ThrottledValueCache::Size() const {
  return cache_.size();
}

}  // namespace maia
