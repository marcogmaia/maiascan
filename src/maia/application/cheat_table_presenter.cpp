// Copyright (c) Maia

#include "maia/application/cheat_table_presenter.h"

namespace maia {

namespace {

template <auto Candidate>
struct SlotTag {};

template <auto Candidate>
constexpr SlotTag<Candidate> Slot = {};

template <typename Storage, typename Sink, typename Receiver, auto Candidate>
void Connect(Storage& storage,
             Sink&& sink,
             Receiver* instance,
             SlotTag<Candidate>) {
  storage.emplace_back(sink.template connect<Candidate>(instance));
};

}  // namespace

CheatTablePresenter::CheatTablePresenter(CheatTableModel& model,
                                         CheatTableView& view)
    : model_(model),
      view_(view) {
  Connect(connections_,
          model_.sinks().TableChanged(),
          this,
          Slot<&CheatTablePresenter::OnTableChanged>);

  Connect(connections_,
          view_.sinks().FreezeToggled(),
          this,
          Slot<&CheatTablePresenter::OnFreezeToggled>);

  Connect(connections_,
          view_.sinks().DescriptionChanged(),
          this,
          Slot<&CheatTablePresenter::OnDescriptionChanged>);

  Connect(connections_,
          view_.sinks().ValueChanged(),
          this,
          Slot<&CheatTablePresenter::OnValueChanged>);

  Connect(connections_,
          view_.sinks().DeleteRequested(),
          this,
          Slot<&CheatTablePresenter::OnDeleteRequested>);
}

void CheatTablePresenter::Render() {
  view_.Render(model_.entries());
}

void CheatTablePresenter::OnTableChanged() {
  // In immediate mode GUI, we usually just re-render, so specific signals
  // might not be strictly necessary unless we had cached state.
  // But good for notification logic.
}

void CheatTablePresenter::OnFreezeToggled(size_t index) {
  model_.ToggleFreeze(index);
}

void CheatTablePresenter::OnDescriptionChanged(size_t index,
                                               std::string new_desc) {
  model_.UpdateEntryDescription(index, new_desc);
}

void CheatTablePresenter::OnValueChanged(size_t index, std::string new_val) {
  model_.SetValue(index, new_val);
}

void CheatTablePresenter::OnDeleteRequested(size_t index) {
  model_.RemoveEntry(index);
}

}  // namespace maia
