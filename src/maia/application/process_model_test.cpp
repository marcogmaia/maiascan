// Copyright (c) Maia

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "maia/application/process_model.h"
#include "maia/tests/fake_process.h"

namespace maia {

class ProcessModelTest : public ::testing::Test {
 public:
  void OnWillDetach() {
    signal_fired = true;
    process_was_valid_in_signal = (model.GetActiveProcess() != nullptr);
  }

 protected:
  ProcessModel model;
  bool signal_fired = false;
  bool process_was_valid_in_signal = false;
};

TEST_F(ProcessModelTest, WillDetachSignalFiredBeforeDestruction) {
  auto process = std::make_unique<test::FakeProcess>();

  // We added SetActiveProcess and GetActiveProcess to ProcessModel.
  model.SetActiveProcess(std::move(process));

  model.sinks().ProcessWillDetach().connect<&ProcessModelTest::OnWillDetach>(
      *this);

  EXPECT_FALSE(signal_fired);

  model.Detach();

  EXPECT_TRUE(signal_fired);
  EXPECT_TRUE(process_was_valid_in_signal);
  EXPECT_EQ(model.GetActiveProcess(), nullptr);
}

}  // namespace maia
