// Copyright (c) Maia

#include "maia/application/cheat_table_presenter.h"

namespace maia {

namespace {

template <auto Candidate>
struct SlotTag {};

template <auto Candidate>
constexpr SlotTag<Candidate> Slot = {};  // NOLINT

template <typename Storage, typename Sink, typename Receiver, auto Candidate>
void Connect(Storage& storage,
             Sink&& sink,
             Receiver* instance,
             SlotTag<Candidate>) {
  storage.emplace_back(
      std::forward<Sink>(sink).template connect<Candidate>(instance));
};

}  // namespace

CheatTablePresenter::CheatTablePresenter(CheatTableModel& model,
                                         CheatTableView& view)
    : model_(model),
      view_(view) {
  // clang-format off
  Connect(connections_, view_.sinks().FreezeToggled(),      &model_, Slot<&CheatTableModel::ToggleFreeze>);
  Connect(connections_, view_.sinks().DescriptionChanged(), &model_, Slot<&CheatTableModel::UpdateEntryDescription>);
  Connect(connections_, view_.sinks().ValueChanged(),       &model_, Slot<&CheatTableModel::SetValue>);
  Connect(connections_, view_.sinks().DeleteRequested(),    &model_, Slot<&CheatTableModel::RemoveEntry>);
  // clang-format on
}

void CheatTablePresenter::Render() {
  view_.Render(model_.entries());
}

}  // namespace maia
