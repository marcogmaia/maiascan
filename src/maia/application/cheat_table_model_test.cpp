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
               std::span<std::byte> out_buffer,
               std::vector<uint8_t>* success_mask),
              (override));

  MOCK_METHOD(bool,
              WriteMemory,

              (uintptr_t address, std::span<const std::byte> buffer),
              (override));

  MOCK_METHOD(std::vector<MemoryRegion>,
              GetMemoryRegions,
              (),
              (const, override));

  MOCK_METHOD(std::vector<mmem::ModuleDescriptor>,
              GetModules,
              (),
              (const, override));

  MOCK_METHOD(bool, Suspend, (), (override));
  MOCK_METHOD(bool, Resume, (), (override));
};

// Test fixture for verifying concurrency fixes
class CheatTableModelTest : public ::testing::Test {
 protected:
  testing::NiceMock<MockProcess> mock_process_;
  CheatTableModel model_;

  void CallWriteMemory(size_t index, const std::vector<std::byte>& data) {
    // We use "1" here because our entry is kInt32.
    // kInt32 will parse "1" into {0x01, 0x00, 0x00, 0x00}.
    // The test mock expects data of size 4 if we use ReadMemory size... wait.
    // The test uses CallWriteMemory(0, data).
    // The logic inside CallWriteMemory originally called private WriteMemory
    // directly. Now we call SetValue("1").
    model_.SetValue(index, "1");
  }
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

  EXPECT_CALL(mock_process_,
              ReadMemory(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([&](std::span<const MemoryAddress> addresses,
                              size_t bytes_per_address,
                              std::span<std::byte> out_buffer,
                              std::vector<uint8_t>* success_mask) {
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
    ASSERT_TRUE(cv_read_start.wait_for(lock, std::chrono::seconds(2), [&] {
      return read_started;
    })) << "Timed out waiting for UpdateValues to start reading";
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

// This test deterministically triggers the race condition where active_process_
// is set to null (by UpdateValues) while WriteMemory is using it.
TEST_F(CheatTableModelTest, WriteMemoryCrashesIfProcessDiesConcurrently) {
  // Synchronization primitives
  std::mutex sync_mutex;
  std::condition_variable cv_paused;
  std::condition_variable cv_resume;
  bool is_paused = false;
  bool should_resume = false;

  // 1. Configure Mock for the race
  std::thread::id victim_thread_id;
  std::thread::id main_thread_id = std::this_thread::get_id();
  std::atomic<bool> victim_started{false};

  // We use WillRepeatedly with a stateful lambda to handle the different
  // threads
  EXPECT_CALL(mock_process_, IsProcessValid())
      .WillRepeatedly(testing::Invoke([&]() {
        auto current_id = std::this_thread::get_id();

        // Background thread (or others): just say it's valid
        if (current_id != victim_thread_id && current_id != main_thread_id) {
          return true;
        }

        // Victim Thread: Pause here to simulate the race window
        if (current_id == victim_thread_id) {
          std::unique_lock lock(sync_mutex);
          is_paused = true;
          cv_paused.notify_one();

          cv_resume.wait(lock, [&] { return should_resume; });
          return true;
        }

        // Main Thread (Attacker): Return false to trigger active_process_ =
        // nullptr
        if (current_id == main_thread_id) {
          return false;
        }

        return true;
      }));

  EXPECT_CALL(mock_process_,
              ReadMemory(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::Return(true));

  // Setup
  model_.SetActiveProcess(&mock_process_);
  model_.AddEntry(0x1234, ScanValueType::kInt32, "Test Entry");

  // Start the Victim Thread
  std::thread victim_thread([&]() {
    while (!victim_started) {
      std::this_thread::yield();
    }
    std::vector<std::byte> data{std::byte{0x1}};
    CallWriteMemory(0, data);
  });

  victim_thread_id = victim_thread.get_id();
  victim_started = true;

  // Wait for Victim to pause inside IsProcessValid
  {
    std::unique_lock lock(sync_mutex);
    // Use wait_for to detect deadlock
    if (!cv_paused.wait_for(
            lock, std::chrono::seconds(5), [&] { return is_paused; })) {
      victim_thread.detach();  // Detach to allow test to fail gracefully
      FAIL() << "Deadlock: Victim thread never reached IsProcessValid";
    }
  }

  // Trigger the Attacker (UpdateValues)
  // This will call IsProcessValid (returning false), and then set
  // active_process_ = nullptr
  model_.UpdateValues();

  // Resume the Victim
  // It will now exit IsProcessValid and try to dereference active_process_
  {
    std::unique_lock lock(sync_mutex);
    should_resume = true;
    cv_resume.notify_one();
  }

  // Wait for crash (or join if it somehow survives)
  victim_thread.join();
}

}  // namespace maia
