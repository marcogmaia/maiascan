// // Copyright (c) Maia

// #include <algorithm>
// #include <numbers>
// #include <ranges>
// #include <vector>

// #include <gmock/gmock.h>
// #include <gtest/gtest.h>

// #include "i_process.h"
// #include "scan_result.h"

// namespace maia {
// namespace {

// using testing::ElementsAre;
// using testing::IsEmpty;

// std::shared_ptr<const MemorySnapshot> CreateSnapshot(
//     std::vector<uintptr_t> addresses, std::vector<std::byte> values) {
//   auto snapshot = std::make_shared<MemorySnapshot>();
//   snapshot->addresses = std::move(addresses);
//   snapshot->values = std::move(values);
//   return snapshot;
// }

// TEST(ScanResultTest, DefaultConstructIsEmpty) {
//   ScanResult result;
//   EXPECT_TRUE(result.empty());
//   EXPECT_EQ(result.size(), 0);
//   EXPECT_THAT(result.addresses(), IsEmpty());
// }

// TEST(ScanResultTest, FromSnapshotInitializesCorrectly) {
//   auto addresses = std::vector<uintptr_t>{0x1000, 0x2000, 0x3000};
//   auto values = std::vector<std::byte>(12);
//   reinterpret_cast<int32_t*>(values.data())[0] = 42;
//   reinterpret_cast<int32_t*>(values.data())[1] = 99;
//   reinterpret_cast<int32_t*>(values.data())[2] = -1;

//   auto snapshot = CreateSnapshot(addresses, values);
//   auto result = ScanResult::FromSnapshot<int32_t>(snapshot);

//   EXPECT_FALSE(result.empty());
//   EXPECT_EQ(result.size(), 3);
//   EXPECT_THAT(result.addresses(), ElementsAre(0x1000, 0x2000, 0x3000));
// }

// TEST(ScanResultTest, ValuesRangeYieldsCorrectValues) {
//   auto addresses = std::vector<uintptr_t>{0x1000, 0x2000, 0x3000};
//   auto values = std::vector<std::byte>(12);
//   auto* ints = reinterpret_cast<int32_t*>(values.data());
//   ints[0] = 10;
//   ints[1] = 20;
//   ints[2] = 30;

//   auto snapshot = CreateSnapshot(addresses, values);
//   auto result = ScanResult::FromSnapshot<int32_t>(snapshot);

//   std::vector<int32_t> actual;
//   std::ranges::copy(result.values<int32_t>(), std::back_inserter(actual));
//   EXPECT_THAT(actual, ElementsAre(10, 20, 30));
// }

// TEST(ScanResultTest, ValuesWorksWithStandardAlgorithms) {
//   auto addresses = std::vector<uintptr_t>{0x1000, 0x2000, 0x3000, 0x4000};
//   auto values = std::vector<std::byte>(16);
//   auto* ints = reinterpret_cast<int32_t*>(values.data());
//   ints[0] = 5;
//   ints[1] = 10;
//   ints[2] = 15;
//   ints[3] = 20;

//   auto snapshot = CreateSnapshot(addresses, values);
//   auto result = ScanResult::FromSnapshot<int32_t>(snapshot);
//   auto values_view = result.values<int32_t>();

//   EXPECT_EQ(std::ranges::count(values_view, 10), 1);
//   auto it = std::ranges::find(values_view, 15);
//   ASSERT_NE(it, values_view.end());  // ✓ Works: values_view is lvalue
//   EXPECT_EQ(*it, 15);

//   EXPECT_TRUE(
//       std::ranges::any_of(values_view, [](int32_t v) { return v > 12; }));
// }

// TEST(ScanResultTest, AsConvertsTypeWithoutCopyingSnapshot) {
//   auto addresses = std::vector<uintptr_t>{0x1000, 0x2000};
//   auto values = std::vector<std::byte>(8);
//   auto* ints = reinterpret_cast<int32_t*>(values.data());
//   ints[0] = 0x3F800000;  // 1.0f as int32
//   ints[1] = 0x40000000;  // 2.0f as int32

//   auto snapshot = CreateSnapshot(addresses, values);
//   auto int_result = ScanResult::FromSnapshot<int32_t>(snapshot);
//   auto float_result = int_result.As<float>();

//   EXPECT_EQ(float_result.size(), 2);
//   EXPECT_THAT(float_result.addresses(), ElementsAre(0x1000, 0x2000));
//   EXPECT_EQ(int_result.addresses().data(),
//             float_result.addresses().data());  // Same snapshot

//   std::vector<float> actual;
//   std::ranges::copy(float_result.values<float>(),
//   std::back_inserter(actual)); EXPECT_THAT(actual, ElementsAre(1.0f, 2.0f));
// }

// // Death test for type mismatch
// TEST(ScanResultDeathTest, WrongTypeAccessAsserts) {
//   auto addresses = std::vector<uintptr_t>{0x1000};
//   auto values = std::vector<std::byte>(4);
//   auto snapshot = CreateSnapshot(addresses, values);
//   auto result = ScanResult::FromSnapshot<int32_t>(snapshot);

//   EXPECT_DEATH(
//       {
//         for (float f : result.values<uint64_t>()) {
//           static_cast<void>(f);
//         }
//       },
//       ".*");
// }

// TEST(ScanResultTest, EmptySnapshotWorksNormally) {
//   auto snapshot = CreateSnapshot({}, {});
//   auto result = ScanResult::FromSnapshot<double>(snapshot);

//   EXPECT_TRUE(result.empty());
//   EXPECT_EQ(result.size(), 0);
//   EXPECT_THAT(result.addresses(), IsEmpty());

//   std::vector<double> values;
//   std::ranges::copy(result.values<double>(), std::back_inserter(values));
//   EXPECT_THAT(values, IsEmpty());
// }

// TEST(ScanResultTest, ConstCorrectness) {
//   const auto addresses = std::vector<uintptr_t>{0x1000, 0x2000};
//   auto values = std::vector<std::byte>(16);
//   reinterpret_cast<int64_t*>(values.data())[0] = 0xCAFEBABE;
//   reinterpret_cast<int64_t*>(values.data())[1] = 0xFEEDFACECAFEBEEF;

//   auto snapshot = CreateSnapshot(addresses, values);
//   const ScanResult result = ScanResult::FromSnapshot<int64_t>(snapshot);

//   EXPECT_EQ(result.size(), 2);
//   EXPECT_FALSE(result.empty());
//   EXPECT_THAT(result.addresses(), ElementsAre(0x1000, 0x2000));

//   std::vector<int64_t> vals;
//   std::ranges::copy(result.values<int64_t>(), std::back_inserter(vals));
//   EXPECT_THAT(vals, ElementsAre(0xCAFEBABE, 0xFEEDFACECAFEBEEF));
// }

// TEST(ScanResultTest, ZipAddressesWithValues) {
//   auto addresses = std::vector<uintptr_t>{0x1000, 0x2000};
//   auto values = std::vector<std::byte>(8);
//   reinterpret_cast<int32_t*>(values.data())[0] = 100;
//   reinterpret_cast<int32_t*>(values.data())[1] = 200;

//   auto snapshot = CreateSnapshot(addresses, values);
//   auto result = ScanResult::FromSnapshot<int32_t>(snapshot);

//   std::vector<std::pair<uintptr_t, int32_t>> pairs;
//   for (const auto& [addr, val] :
//        std::views::zip(result.addresses(), result.values<int32_t>())) {
//     pairs.emplace_back(addr, val);
//   }

//   EXPECT_THAT(pairs,
//               ElementsAre(std::pair{0x1000, 100}, std::pair{0x2000, 200}));
// }

// // GMock for IProcess
// class MockProcess : public IProcess {
//  public:
//   MOCK_METHOD(bool,
//               ReadMemory,
//               (std::span<const MemoryAddress> addresses,
//                size_t bytes_per_address,
//                std::span<std::byte> out_buffer),
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

// TEST(ReadAtTest, ReadsTypedValueFromProcess) {
//   MockProcess process;
//   const uintptr_t addr = 0x1000;
//   const uint32_t expected = 0xDEADBEEF;

//   EXPECT_CALL(process, ReadMemory(addr, testing::_))
//       .WillOnce([&expected](uintptr_t, std::span<std::byte> buffer) {
//         std::memcpy(buffer.data(), &expected, sizeof(expected));
//         return true;
//       });

//   auto val = ReadAt<uint32_t>(process, addr);
//   EXPECT_EQ(val, expected);
// }

// TEST(WriteAtTest, WritesTypedValueToProcess) {
//   MockProcess process;
//   const uintptr_t addr = 0x2000;
//   const float value = std::numbers::pi_v<float>;

//   EXPECT_CALL(process, WriteMemory(addr, testing::_))
//       .WillOnce([&value](uintptr_t, std::span<const std::byte> buffer) {
//         float received;
//         std::memcpy(&received, buffer.data(), sizeof(received));
//         EXPECT_FLOAT_EQ(received, value);
//         return true;
//       });

//   EXPECT_TRUE(WriteAt<float>(process, addr, value));
// }

// }  // namespace
// }  // namespace maia
