// Copyright (c) Maia

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stop_token>

#include "maia/core/scanner.h"
#include "maia/tests/fake_process.h"

namespace maia::core {
namespace {

class ScannerIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    process_ = std::make_unique<test::FakeProcess>(1024);
  }

  std::unique_ptr<test::FakeProcess> process_;
  Scanner scanner_;
};

TEST_F(ScannerIntegrationTest, FirstScanExactValueFindsMatches) {
  // Write test values
  process_->WriteValue<uint32_t>(0, 42);
  process_->WriteValue<uint32_t>(4, 42);
  process_->WriteValue<uint32_t>(8, 99);  // Different value

  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kExactValue;
  config.value = {std::byte{42}, std::byte{0}, std::byte{0}, std::byte{0}};
  config.alignment = 4;

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.storage.addresses.size(), 2);
}

TEST_F(ScannerIntegrationTest,
       FirstScanUnknownValueCapturesAllAlignedAddresses) {
  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kUnknown;
  config.alignment = 4;

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_TRUE(result.success);
  // Should capture all 4-byte aligned addresses
  const size_t expected_count = 1024 / 4;  // 256
  EXPECT_EQ(result.storage.addresses.size(), expected_count);
}

TEST_F(ScannerIntegrationTest, FirstScanWithMaskRespectsMask) {
  process_->WriteValue<uint32_t>(0, 0x12345678);
  process_->WriteValue<uint32_t>(4, 0xABCD5678);  // Last 2 bytes match pattern

  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kExactValue;
  config.value = {
      std::byte{0x78}, std::byte{0x56}, std::byte{0x00}, std::byte{0x00}};
  config.mask = {
      std::byte{0xFF}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x00}};
  config.alignment = 4;

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.storage.addresses.size(),
            2);  // Both match the masked pattern
}

TEST_F(ScannerIntegrationTest, FirstScanInvalidConfigReturnsError) {
  ScanConfig config;
  config.alignment = 0;  // Invalid

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ScannerIntegrationTest, FirstScanInvalidProcessReturnsError) {
  // Create an invalid process by resetting the unique_ptr
  process_.reset();
  process_ = std::make_unique<test::FakeProcess>(0);  // Empty process

  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kUnknown;

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_TRUE(result.success);  // Empty process is valid, just has no regions
  EXPECT_TRUE(result.storage.addresses.empty());
}

TEST_F(ScannerIntegrationTest, FirstScanStopTokenCancelsScan) {
  std::stop_source stop_source;
  stop_source.request_stop();

  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kUnknown;

  auto result = scanner_.FirstScan(*process_, config, stop_source.get_token());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message, "Scan cancelled");
}

TEST_F(ScannerIntegrationTest, FirstScanProgressCallbackCalled) {
  int callback_count = 0;
  auto progress_callback = [&callback_count](float progress) {
    ++callback_count;
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
  };

  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kUnknown;

  scanner_.SetChunkSize(256);  // Smaller chunks to ensure multiple callbacks
  auto result = scanner_.FirstScan(*process_, config, {}, progress_callback);

  EXPECT_TRUE(result.success);
  EXPECT_GT(callback_count, 0);
}

TEST_F(ScannerIntegrationTest, FirstScanExactValueRequiresValue) {
  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kExactValue;
  config.value.clear();  // Empty value

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ScannerIntegrationTest, FirstScanNextScanWorkflow) {
  // First scan - unknown
  ScanConfig first_config;
  first_config.value_type = ScanValueType::kUInt32;
  first_config.comparison = ScanComparison::kUnknown;
  first_config.alignment = 4;

  auto first_result = scanner_.FirstScan(*process_, first_config);
  EXPECT_TRUE(first_result.success);
  EXPECT_FALSE(first_result.storage.addresses.empty());

  // Change a value
  process_->WriteValue<uint32_t>(0, 999);

  // Next scan - changed
  ScanConfig next_config;
  next_config.value_type = ScanValueType::kUInt32;
  next_config.comparison = ScanComparison::kChanged;
  next_config.alignment = 4;

  auto next_result =
      scanner_.NextScan(*process_, next_config, first_result.storage);
  EXPECT_TRUE(next_result.success);
  EXPECT_EQ(next_result.storage.addresses.size(),
            1);  // Only the changed address
}

TEST_F(ScannerIntegrationTest, FirstScanWithDifferentAlignments) {
  ScanConfig config;
  config.value_type = ScanValueType::kUInt32;
  config.comparison = ScanComparison::kUnknown;
  config.alignment = 8;  // 8-byte alignment

  auto result = scanner_.FirstScan(*process_, config);

  EXPECT_TRUE(result.success);
  const size_t expected_count = 1024 / 8;  // 128
  EXPECT_EQ(result.storage.addresses.size(), expected_count);
}

}  // namespace
}  // namespace maia::core
