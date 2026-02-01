// Copyright (c) Maia

#pragma once

#include <utility>

namespace maia {

/// \brief Internal tag type used for compile-time function pointer deduction.
template <auto Candidate>
struct SlotTag {};

/// \brief Compile-time constant wrapper for function pointers.
/// \details Usage: maia::Slot<&Class::Method> or maia::Slot<&FreeFunction>.
template <auto Candidate>
constexpr SlotTag<Candidate> Slot = {};  // NOLINT

/// \brief Connects a signal to a member function and manages connection
/// lifetime.
/// \param storage Container to hold the scoped connection (e.g., std::vector).
/// \param sink The source signal or sink.
/// \param instance The receiver object instance.
/// \param tag The slot wrapper (use maia::Slot<&Class::Method>).
template <typename Storage, typename Sink, typename Receiver, auto Candidate>
void Connect(Storage& storage,
             Sink&& sink,
             Receiver* instance,
             SlotTag<Candidate>) {
  storage.emplace_back(
      std::forward<Sink>(sink).template connect<Candidate>(instance));
};

/// \brief Connects a signal to a free function or static method.
/// \param storage Container to hold the scoped connection.
/// \param sink The source signal or sink.
/// \param tag The slot wrapper (use maia::Slot<&Function>).
template <typename Storage, typename Sink, auto Candidate>
void Connect(Storage& storage, Sink&& sink, SlotTag<Candidate>) {
  storage.emplace_back(std::forward<Sink>(sink).template connect<Candidate>());
}

}  // namespace maia
