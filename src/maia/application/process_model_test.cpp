// Copyright (c) Maia

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "maia/application/process_model.h"
#include "maia/tests/fake_process.h"

namespace maia {

class ProcessModelTest : public ::testing::Test {
 public:
  void OnWillDetach() {
    signal_fired_ = true;
    process_was_valid_in_signal_ = (model_.GetActiveProcess() != nullptr);
  }

 protected:
  ProcessModel model_;
  bool signal_fired_ = false;
  bool process_was_valid_in_signal_ = false;
};

TEST_F(ProcessModelTest, WillDetachSignalFiredBeforeDestruction) {
  auto process = std::make_unique<test::FakeProcess>();

  // We added SetActiveProcess and GetActiveProcess to ProcessModel.
  model_.SetActiveProcess(std::move(process));

  model_.sinks().ProcessWillDetach().connect<&ProcessModelTest::OnWillDetach>(
      *this);

  EXPECT_FALSE(signal_fired_);

  model_.Detach();

  EXPECT_TRUE(signal_fired_);
  EXPECT_TRUE(process_was_valid_in_signal_);
  EXPECT_EQ(model_.GetActiveProcess(), nullptr);
}

}  // namespace maia
