// // Copyright (c) Maia

// #include <gmock/gmock.h>
// #include <gtest/gtest.h>
// #include <cstdint>
// #include <span>
// #include <vector>

// #include "maia/core/i_process.h"
// #include "maia/core/memory_snapshot.h"

// namespace maia {

// using ::testing::_;
// using ::testing::DoAll;
// using ::testing::Return;
// using ::testing::SetArrayArgument;

// class MockIProcess : public IProcess {
//  public:
//   MOCK_METHOD(bool,
//               ReadMemory,
//               (std::span<const MemoryAddress>, size_t, std::span<std::byte>),
//               (override));
//   MOCK_METHOD(bool,
//               WriteMemory,
//               (uintptr_t, std::span<const std::byte>),
//               (override));
//   MOCK_METHOD(std::vector<MemoryRegion>,
//               GetMemoryRegions,
//               (),
//               (const, override));
//   MOCK_METHOD(uint32_t, GetProcessId, (), (const, override));
//   MOCK_METHOD(std::string, GetProcessName, (), (const, override));
//   MOCK_METHOD(bool, IsProcessValid, (), (const, override));
//   MOCK_METHOD(uintptr_t, GetBaseAddress, (), (const, override));
// };

// class SnapshotTest : public ::testing::Test {
//  protected:
//   // Convert uint8_t vector to std::byte vector (for setting mock
//   expectations) static std::vector<std::byte> ToBytes(const
//   std::vector<uint8_t>& values) {
//     std::vector<std::byte> result;
//     result.reserve(values.size());
//     for (auto v : values) {
//       result.push_back(static_cast<std::byte>(v));
//     }
//     return result;
//   }

//   // Convert std::byte vector to uint8_t vector (for assertions)
//   static std::vector<uint8_t> FromBytes(const std::vector<std::byte>& bytes)
//   {
//     std::vector<uint8_t> result;
//     result.reserve(bytes.size());
//     for (auto b : bytes) {
//       result.push_back(static_cast<uint8_t>(b));
//     }
//     return result;
//   }

//   MockIProcess mock_process_;
//   Snapshot snapshot_{mock_process_};
// };

// // Test that ReadMemory is called with correct parameters
// TEST_F(SnapshotTest, CallsReadMemoryWithCorrectParameters) {
//   // This test requires that UpdateFromPrevious() has been called to populate
//   // addresses and value_size. For illustration, we assume it's been done.

//   // Arrange: Set up mock to return some data
//   std::vector<uint8_t> dummy_data = {1, 2, 3, 4};
//   EXPECT_CALL(mock_process_, ReadMemory(_, 4, _))
//       .WillOnce(DoAll(SetArrayArgument<2>(ToBytes(dummy_data).begin(),
//                                           ToBytes(dummy_data).end()),
//                       Return(true)));

//   // Act
//   snapshot_.ScanChanged();

//   // Assert: Verify the call was made (handled by gmock)
// }

// // Test behavior when memory changes are detected
// TEST_F(SnapshotTest, UpdatesValuesWhenMemoryChanges) {
//   // Arrange: This test assumes we can populate initial state via public API
//   // (e.g., UpdateFromPrevious or similar). For demonstration:

//   // First scan: Set up initial state
//   std::vector<uint8_t> initial_values = {10, 20, 30, 40};
//   EXPECT_CALL(mock_process_, ReadMemory(_, _, _))
//       .WillOnce(DoAll(SetArrayArgument<2>(ToBytes(initial_values).begin(),
//                                           ToBytes(initial_values).end()),
//                       Return(true)));
//   snapshot_.ScanChanged();  // Now previous_layer_ contains initial_values

//   // Second scan: Memory changed
//   std::vector<uint8_t> changed_values = {10, 20, 99, 40};  // Third byte
//   changed EXPECT_CALL(mock_process_, ReadMemory(_, _, _))
//       .WillOnce(DoAll(SetArrayArgument<2>(ToBytes(changed_values).begin(),
//                                           ToBytes(changed_values).end()),
//                       Return(true)));

//   // Act
//   snapshot_.ScanChanged();

//   // Assert: Verify current layer contains new values
//   auto current_layer = snapshot_.get_snapshot();
//   auto values = FromBytes(current_layer.values);

//   // Note: Since get_snapshot() returns by value, it captures state at call
//   time
//   // This makes timing-dependent tests fragile
//   EXPECT_EQ(values.size(), changed_values.size());
//   EXPECT_EQ(values[2], 99);  // Changed value should be present
// }

// // Test ReadMemory failure handling
// TEST_F(SnapshotTest, HandlesReadMemoryFailureGracefully) {
//   // Arrange
//   EXPECT_CALL(mock_process_, ReadMemory(_, _, _)).WillOnce(Return(false));

//   // Act & Assert: Should not crash
//   EXPECT_NO_THROW(snapshot_.ScanChanged());
// }

// // Test that layer swap occurs correctly
// TEST_F(SnapshotTest, SwapsPreviousAndCurrentLayers) {
//   // This is difficult to verify with value-returning getters because
//   // we cannot observe the internal state change directly.

//   // Arrange: Perform first scan
//   std::vector<uint8_t> first_scan = {1, 2, 3, 4};
//   EXPECT_CALL(mock_process_, ReadMemory(_, _, _))
//       .WillOnce(DoAll(SetArrayArgument<2>(ToBytes(first_scan).begin(),
//                                           ToBytes(first_scan).end()),
//                       Return(true)));
//   snapshot_.ScanChanged();

//   auto after_first_prev = snapshot_.get_previous();
//   auto after_first_curr = snapshot_.get_snapshot();

//   // Act: Perform second scan with different values
//   std::vector<uint8_t> second_scan = {5, 6, 7, 8};
//   EXPECT_CALL(mock_process_, ReadMemory(_, _, _))
//       .WillOnce(DoAll(SetArrayArgument<2>(ToBytes(second_scan).begin(),
//                                           ToBytes(second_scan).end()),
//                       Return(true)));
//   snapshot_.ScanChanged();

//   // Assert: Previous should now contain what was current
//   auto after_second_prev = snapshot_.get_previous();
//   auto after_second_curr = snapshot_.get_snapshot();

//   // Compare values (addresses should be the same)
//   EXPECT_EQ(FromBytes(after_second_prev.values),
//             FromBytes(after_first_curr.values));
// }

// }  // namespace maia
