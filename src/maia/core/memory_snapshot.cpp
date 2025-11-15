// Copyright (c) Maia

#include "maia/core/memory_snapshot.h"

#include <algorithm>
#include <ranges>

#include "maia/core/i_process.h"

namespace maia {

void Snapshot::ScanChanged() {
  const size_t value_size = current_layer_.value_size;
  std::swap(previous_layer_, current_layer_);
  std::vector<std::byte> buffer(current_layer_.addresses.size() * value_size);
  accessor_.ReadMemory(current_layer_.addresses, value_size, buffer);

  auto new_vals = std::views::chunk(buffer, value_size);
  auto prev_vals = std::views::chunk(previous_layer_.values, value_size);
  auto curr_vals = std::views::chunk(current_layer_.values, value_size);

  for (const auto& [new_chunk, prev_chunk, curr_chunk] :
       std::views::zip(new_vals, prev_vals, curr_vals)) {
    if (!std::ranges::equal(new_chunk, prev_chunk)) {
      std::ranges::copy(new_chunk, curr_chunk.begin());
    }
  }
}

void Snapshot::UpdateFromPrevious() {
  auto size = previous_layer_.addresses.size() * previous_layer_.value_size;
  std::vector<std::byte> buffer(size);
  std::swap(previous_layer_, current_layer_);
  accessor_.ReadMemory(
      current_layer_.addresses, current_layer_.value_size, buffer);
}

}  // namespace maia
