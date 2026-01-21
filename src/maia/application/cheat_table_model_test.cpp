// Copyright (c) Maia

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <span>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "maia/application/cheat_table_model.h"
#include "maia/core/i_process.h"

namespace maia {
namespace {

using namespace std::chrono_literals;

class MockProcess : public IProcess {
 public:
  // IProcess Interface
  MOCK_METHOD(bool, IsProcessValid, (), (const, override));
  MOCK_METHOD(std::string, GetProcessName, (), (const, override));
  MOCK_METHOD(uint32_t, GetProcessId, (), (const, override));
  MOCK_METHOD(uintptr_t, GetBaseAddress, (), (const, override));

  MOCK_METHOD(bool,
              ReadMemory,
              (std::span<const MemoryAddress> addresses,
               size_t bytes_per_address,
               std::span<std::byte> out_buffer),
              (override));

  MOCK_METHOD(bool,
              WriteMemory,
              (uintptr_t address, std::span<const std::byte> buffer),
              (override));

  MOCK_METHOD(std::vector<MemoryRegion>,
              GetMemoryRegions,
              (),
              (const, override));
};

// Test fixture for verifying concurrency fixes
class CheatTableModelTest : public ::testing::Test {
 protected:
  testing::NiceMock<MockProcess> mock_process_;
  CheatTableModel model_;
};

TEST_F(CheatTableModelTest, RaceConditionLostUpdateIsPrevented) {
  // Synchronization primitives
  std::mutex mutex;
  std::condition_variable cv_read_start;
  std::condition_variable cv_resume_read;
  bool read_started = false;
  bool allow_resume = false;
  std::atomic<bool> should_block{false};

  // Configure Mock
  EXPECT_CALL(mock_process_, IsProcessValid())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(mock_process_, ReadMemory(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([&](std::span<const MemoryAddress> addresses,
                              size_t bytes_per_address,
                              std::span<std::byte> out_buffer) {
            if (!should_block) {
              return true;
            }

            // Signal that read has started
            {
              std::unique_lock lock(mutex);
              read_started = true;
              cv_read_start.notify_one();
            }

            // Wait for permission to finish
            {
              std::unique_lock lock(mutex);
              cv_resume_read.wait(lock, [&] { return allow_resume; });
            }
            return true;
          }));

  model_.SetActiveProcess(&mock_process_);
  model_.AddEntry(0x1000, ScanValueType::kInt32, "Entry 1");

  ASSERT_EQ(model_.entries()->size(), 1);

  // Enable blocking only for the background thread
  should_block = true;

  // 3. Trigger the background update (manually for control)
  // We use a separate thread to mimic the background loop behavior
  std::thread background_thread([&] { model_.UpdateValues(); });

  // 4. Wait for the background thread to be "inside" the update loop
  {
    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv_read_start.wait_for(lock, 2s, [&] { return read_started; }))
        << "Timed out waiting for UpdateValues to start reading";
  }

  // At this point, the scanner has captured the snapshot of size 1.
  // In the buggy implementation, it holds a copy of the vector with 1 entry.

  // Disable blocking for the attack call (so it doesn't deadlock itself)
  should_block = false;

  // 5. ATTACK: Add a new entry from the main thread
  model_.AddEntry(0x2000, ScanValueType::kInt32, "Entry 2 (The Victim)");
  ASSERT_EQ(model_.entries()->size(), 2)
      << "Entry should be added immediately to the model";

  // Re-enable blocking to keep the test consistent if resuming background
  // thread does more reads? No, Resume read just finishes the current read.

  // 6. Release the background thread
  {
    std::unique_lock lock(mutex);
    allow_resume = true;
    cv_resume_read.notify_one();
  }

  background_thread.join();

  // 7. ASSERT: Did we lose the entry?
  auto entries = model_.entries();
  EXPECT_EQ(entries->size(), 2)
      << "CRITICAL FAIL: The background loop overwrote the added entry!";

  if (entries->size() >= 2) {
    EXPECT_EQ(entries->at(1).description, "Entry 2 (The Victim)");
  }
}

}  // namespace
}  // namespace maia
