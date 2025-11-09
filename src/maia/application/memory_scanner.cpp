// // Copyright (c) Maia

// #include "memory_scanner.h"  // (Adjust path as needed)

// #include <algorithm>
// #include <ranges>

// #include "maia/logging.h"  // (Assuming you have this)

// namespace maia {

// // --- Public Interface ---

// MemoryScanner::MemoryScanner(IProcess& process)
//     : process_(process) {
//   // Get an initial snapshot of memory regions.
//   // UpdateMemoryRegions();
// }

// void MemoryScanner::UpdateMemoryRegions() {
//   if (process_.IsProcessValid()) {
//     memory_regions_ = process_.GetMemoryRegions();
//   } else {
//     memory_regions_.clear();
//   }
// }

// ScanResult MemoryScanner::NewScan(ScanValueType value_type,
//                                      const ScanParams& params) {
//   if (!process_.IsProcessValid()) {
//     LogWarning("Process is invalid.");
//     return {};
//   }

//   // On a new scan, it's a good idea to refresh the memory map.
//   UpdateMemoryRegions();

//   ScanResult result_set;
//   result_set.value_type = value_type;

//   // This lambda calls our region scanner helper.
//   auto scan_region_func = [this, value_type, &params](const MemoryRegion&
//   reg) {
//     return ScanRegionForNewScan(reg, value_type, params);
//   };

//   // Use C++20 ranges to scan all regions and join the results.
//   auto view = memory_regions_ | std::views::transform(scan_region_func);
//   result_set.addresses =
//       std::views::join(view) | std::ranges::to<std::vector<ScanResult>>();

//   return result_set;
// }

// ScanResult MemoryScanner::NextScan(const ScanResult& previous_set,
//                                       const ScanParams& params) {
//   if (!process_.IsProcessValid()) {
//     LogWarning("Process is invalid.");
//     return {};
//   }

//   ScanResult new_set;
//   new_set.value_type = previous_set.value_type;

//   // Reserve space to avoid many reallocations.
//   new_set.addresses.reserve(previous_set.GetCount());

//   // This lambda calls our filter helper for each prior result.
//   auto filter_result_func = [this,
//                              value_type = previous_set.value_type,
//                              &params](const ScanResult& res) {
//     return FilterResultForNextScan(res, value_type, params);
//   };

//   // Iterate all previous results, transform them (filter),
//   // drop the empty std::nullopts, and unwrap the remaining ones.
//   auto view =
//       previous_set.addresses | std::views::transform(filter_result_func) |
//       std::views::filter([](const auto& opt) { return opt.has_value(); }) |
//       std::views::transform([](const auto& opt) { return *opt; });

//   new_set.addresses = view | std::ranges::to<std::vector<ScanResult>>();

//   return new_set;
// }

// // --- New Scan Helpers ---

// // Size of the buffer to use when reading memory.
// // 1MB is a good balance.
// constexpr size_t kScanChunkSize = 1024 * 1024;

// MemoryScanner::ScanResultVec MemoryScanner::ScanRegionForNewScan(
//     const MemoryRegion& region,
//     ScanValueType value_type,
//     const ScanParams& params) {
//   ScanResultVec addresses_found;
//   std::vector<std::byte> chunk(kScanChunkSize);

//   size_t type_size = GetValueTypeSize(value_type);
//   // Note: This logic needs to be much more complex for dynamic types
//   // like strings or byte arrays, which can span across chunk boundaries.
//   // For fixed-size types, we just need a small overlap.
//   size_t overlap = (type_size > 1) ? (type_size - 1) : 0;

//   for (uintptr_t offset = 0; offset < region.size;
//        offset += (kScanChunkSize - overlap)) {
//     size_t read_size = std::min(kScanChunkSize, region.size - offset);
//     if (read_size == 0) {
//       break;
//     }

//     uintptr_t current_address = region.base_address + offset;

//     // Resize chunk to the exact size we need to read
//     chunk.resize(read_size);

//     if (!process_.ReadMemory(current_address, chunk)) {
//       // Failed to read this chunk, skip it
//       continue;
//     }

//     // Find all matches in the chunk we just read
//     FindMatchesInChunk(
//         addresses_found, chunk, current_address, value_type, params);
//   }

//   return addresses_found;
// }

// void MemoryScanner::FindMatchesInChunk(ScanResultVec& results,
//                                        const std::vector<std::byte>& chunk,
//                                        uintptr_t chunk_base_address,
//                                        ScanValueType value_type,
//                                        const ScanParams& params) {
//   // This is the "hot loop."
//   // It needs to be specialized for every type/scan combo.

//   // --- Example: kU32, kExactValue ---
//   if (value_type == ScanValueType::kU32 &&
//       params.type == ScanType::kExactValue) {
//     // 1. Get the target value.
//     // (Assumes value1 is at least 4 bytes, add error checking)
//     if (params.value1.size() < 4) {
//       return;
//     }
//     const uint32_t target_value =
//         *reinterpret_cast<const uint32_t*>(params.value1.data());

//     // 2. Iterate the chunk, checking every possible 4-byte alignment.
//     // We stop 3 bytes early to avoid reading past the end.
//     for (size_t i = 0; i <= chunk.size() - 4; ++i) {
//       // Reinterpret the bytes at this offset as a U32.
//       // This is safe because we're just reading from our local vector.
//       const uint32_t memory_value =
//           *reinterpret_cast<const uint32_t*>(&chunk[i]);

//       // 3. Compare
//       if (memory_value == target_value) {
//         // 4. Found a match. Create and store the result.
//         ScanResult res;
//         res.address = chunk_base_address + i;

//         // Snapshot the value.
//         res.value.resize(4);
//         std::memcpy(res.value.data(), &chunk[i], 4);

//         results.push_back(std::move(res));
//       }
//     }
//   }
//   // --- Example: kU32, kGreaterThan ---
//   else if (value_type == ScanValueType::kU32 &&
//            params.type == ScanType::kGreaterThan) {
//     // ... (similar logic, but use > comparison)
//   }
//   // --- Example: kByteArray, kExactValue ---
//   else if (value_type == ScanValueType::kByteArray &&
//            params.type == ScanType::kExactValue) {
//     // This is the std::search logic from your original code.
//     // (Be careful of matches spanning chunk boundaries!)
//   }

//   // ...
//   // TODO: Implement all other ScanValueType and ScanType combinations.
//   // ...
// }

// // --- Next Scan Helpers ---

// std::optional<ScanResult> MemoryScanner::FilterResultForNextScan(
//     const ScanResult& previous_result,
//     ScanValueType value_type,
//     const ScanParams& params) {
//   // 1. Read the *current* value from memory.
//   // The size is based on the *previous* scan's snapshot.
//   size_t read_size = previous_result.value.size();
//   if (read_size == 0) {
//     return std::nullopt;  // Should not happen
//   }

//   std::vector<std::byte> current_value_buffer(read_size);

//   if (!process_.ReadMemory(previous_result.address, current_value_buffer)) {
//     // Address is no longer valid, discard it.
//     return std::nullopt;
//   }

//   // 2. Compare the new value with the old one.
//   bool matches = CompareValues(
//       current_value_buffer, previous_result.value, params, value_type);

//   // 3. If it matches, create an updated result.
//   if (matches) {
//     ScanResult new_result;
//     new_result.address = previous_result.address;
//     new_result.value = std::move(current_value_buffer);  // Move the new
//     value return new_result;
//   }

//   return std::nullopt;
// }

// // bool MemoryScanner::CompareValues(std::span<const std::byte>
// current_value,
// //                                   std::span<const std::byte>
// previous_value,
// //                                   const ScanParams& params,
// //                                   ScanValueType value_type) {
// //   // This function is a large switch statement that dispatches
// //   // to templated helper functions.
// //   // This is a common pattern to avoid code duplication.

// //   // --- Example: kChanged ---
// //   if (params.type == ScanType::kChanged) {
// //     // Simple byte-for-byte comparison.
// //     // return current_value != previous_value;
// //     return std::ranges::equal_range(current_value, previous_value);
// //   }

// //   // --- Example: kUnchanged ---
// //   if (params.type == ScanType::kUnchanged) {
// //     return current_value == previous_value;
// //   }

// //   // --- Example: kExactValue (for a "Next Scan") ---
// //   if (params.type == ScanType::kExactValue) {
// //     // Compare the *current* memory value to the *new* value1.
// //     return current_value == params.value1;
// //   }

// //   // --- Example: kIncreased (for kU32) ---
// //   if (params.type == ScanType::kIncreased &&
// //       value_type == ScanValueType::kU32) {
// //     if (current_value.size() < 4 || previous_value.size() < 4) {
// //       return false;
// //     }

// //     uint32_t current = *reinterpret_cast<const
// //     uint32_t*>(current_value.data()); uint32_t previous =
// //         *reinterpret_cast<const uint32_t*>(previous_value.data());

// //     return current > previous;
// //   }

// //   // ...
// //   // TODO: Implement all other ScanType and ScanValueType comparisons.
// //   // (kIncreasedBy, kDecreased, kBetween, etc.)
// //   // ...

// //   return false;  // Default: no match
// // }

// // --- Generic Helpers ---

// size_t MemoryScanner::GetValueTypeSize(ScanValueType value_type) {
//   switch (value_type) {
//     case ScanValueType::kS8:
//     case ScanValueType::kU8:
//       return 1;
//     case ScanValueType::kS16:
//     case ScanValueType::kU16:
//       return 2;
//     case ScanValueType::kS32:
//     case ScanValueType::kU32:
//     case ScanValueType::kF32:
//       return 4;
//     case ScanValueType::kS64:
//     case ScanValueType::kU64:
//     case ScanValueType::kF64:
//       return 8;
//     case ScanValueType::kString:
//     case ScanValueType::kStringW:
//     case ScanValueType::kByteArray:
//     case ScanValueType::kInvalid:
//     default:
//       return 0;  // Dynamic size
//   }
// }

// }  // namespace maia
