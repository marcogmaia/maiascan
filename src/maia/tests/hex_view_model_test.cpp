#include "maia/gui/models/hex_view_model.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "maia/core/memory_common.h"
#include "maia/tests/fake_process.h"

namespace maia::gui::test {

using namespace std::chrono_literals;

class HexViewModelTest : public ::testing::Test {
 protected:
  maia::test::FakeProcess process_;
};

TEST_F(HexViewModelTest, Initialization) {
  HexViewModel model;
  model.SetProcess(&process_);
  EXPECT_EQ(model.GetCurrentAddress(), process_.GetBaseAddress());
}

TEST_F(HexViewModelTest, GoTo) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  EXPECT_EQ(model.GetCurrentAddress(), addr);
}

TEST_F(HexViewModelTest, Scroll) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  model.Scroll(1);
  // Assuming 16 bytes per line
  EXPECT_EQ(model.GetCurrentAddress(), addr + 0x10);

  model.Scroll(-2);
  EXPECT_EQ(model.GetCurrentAddress(), addr - 0x10);
}

TEST_F(HexViewModelTest, SelectionRange) {
  HexViewModel model;
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.SetSelectionRange(addr, addr + 0x10);
  auto range = model.GetSelectionRange();
  EXPECT_EQ(range.start, addr);
  EXPECT_EQ(range.end, addr + 0x10);
}

TEST_F(HexViewModelTest, CachePage) {
  // Fill process memory with some data
  auto& mem = process_.GetRawMemory();
  for (size_t i = 0; i < mem.size(); ++i) {
    mem[i] = static_cast<std::byte>(i & 0xFF);
  }

  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  model.CachePage();

  const auto& cache = model.GetCachedData();
  ASSERT_EQ(cache.size(), 0x1000);  // 4KB
  // Offset from base_address is 0x1000
  EXPECT_EQ(cache[0], static_cast<std::byte>(0x1000 & 0xFF));
  EXPECT_EQ(cache[0xFFF], static_cast<std::byte>(0x1FFF & 0xFF));
}

TEST_F(HexViewModelTest, EditingAndCommit) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1005;
  model.SetByte(addr, std::byte{0xAB});

  // Verify it's NOT yet in process
  std::byte val;
  MemoryAddress m_addr = addr;
  process_.ReadMemory({&m_addr, 1}, 1, {&val, 1}, nullptr);
  EXPECT_NE(val, std::byte{0xAB});

  model.Commit();

  // Verify it IS now in process
  process_.ReadMemory({&m_addr, 1}, 1, {&val, 1}, nullptr);
  EXPECT_EQ(val, std::byte{0xAB});
}

TEST_F(HexViewModelTest, CachePageUnmapped) {
  HexViewModel model;
  model.SetProcess(&process_);
  // Address far outside process memory
  uintptr_t addr =
      process_.GetBaseAddress() + process_.GetRawMemory().size() + 0x1000;
  model.GoTo(addr);
  model.CachePage();

  const auto& cache = model.GetCachedData();
  const auto& mask = model.GetValidityMask();
  ASSERT_EQ(cache.size(), 0x1000);
  ASSERT_EQ(mask.size(), 0x1000);
  // Should be all zeros if unmapped
  for (size_t i = 0; i < 0x1000; ++i) {
    EXPECT_EQ(mask[i], 0);
  }
}

TEST_F(HexViewModelTest, ChangeDetection) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  model.Refresh();

  // Change memory
  process_.GetRawMemory()[0x1000] = std::byte{0xEE};
  model.Refresh();

  const auto& diffs = model.GetDiffMap();
  EXPECT_TRUE(diffs.contains(addr));
}

TEST_F(HexViewModelTest, NoChangeNoUpdate) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  model.Refresh();

  // No change
  model.Refresh();

  const auto& diffs = model.GetDiffMap();
  EXPECT_TRUE(diffs.empty());
}

TEST_F(HexViewModelTest, ScrollClearsDiffs) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  model.Refresh();

  process_.GetRawMemory()[0x1000] = std::byte{0xEE};
  model.Refresh();
  EXPECT_FALSE(model.GetDiffMap().empty());

  model.Scroll(1);
  EXPECT_TRUE(model.GetDiffMap().empty());

  model.GoTo(addr + 0x20);
  EXPECT_TRUE(model.GetDiffMap().empty());
}

TEST_F(HexViewModelTest, ReadValue) {
  auto& mem = process_.GetRawMemory();
  mem[0x1000] = std::byte{0x01};
  mem[0x1001] = std::byte{0x02};
  mem[0x1002] = std::byte{0x03};
  mem[0x1003] = std::byte{0x04};

  HexViewModel model;
  model.SetProcess(&process_);
  model.GoTo(process_.GetBaseAddress() + 0x1000);
  model.Refresh();

  uint32_t val = 0;
  EXPECT_TRUE(model.ReadValue(process_.GetBaseAddress() + 0x1000,
                              sizeof(val),
                              reinterpret_cast<std::byte*>(&val)));
  EXPECT_EQ(val, 0x04030201);
}

TEST_F(HexViewModelTest, Pruning) {
  HexViewModel model;
  model.SetProcess(&process_);
  uintptr_t addr = process_.GetBaseAddress() + 0x1000;
  model.GoTo(addr);
  model.Refresh();

  process_.GetRawMemory()[0x1000] = std::byte{0xEE};
  model.Refresh();
  EXPECT_FALSE(model.GetDiffMap().empty());

  // TODO(marco): Fix this, we shouldn't do thread sleep in unit test. Consider
  // this as broken. Wait for 2.1s to ensure pruning happens
  std::this_thread::sleep_for(2100ms);
  model.Refresh();
  EXPECT_TRUE(model.GetDiffMap().empty());
}

}  // namespace maia::gui::test
