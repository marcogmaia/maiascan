// Copyright (c) Maia

#pragma once

#include <concepts>
#include <iterator>

namespace maia {

/// \brief Concept for a valid transform_if operation using iterators.
///
/// \details Checks if a predicate and a transform function can be applied
///          to an input iterator's element, and if the transform's result
///          can be written to an output iterator.
///
/// \tparam I The input iterator type.
/// \tparam O The output iterator type.
/// \tparam Func The transformation function type.
/// \tparam Pred The predicate function type.
template <typename I, typename O, typename Func, typename Pred>
concept CTransformIfAbleFromIterators =
    // 'Pred' must be a predicate callable with the input element's type
    std::predicate<Pred, std::iter_reference_t<I>> &&
    // 'Func' must be invocable with the input element's type
    std::invocable<Func, std::iter_reference_t<I>> &&
    // 'O' must be an output iterator that can accept the *result*
    // of the transformation function
    std::output_iterator<O,
                         std::invoke_result_t<Func, std::iter_reference_t<I>>>;

/// \brief Transforms and copies elements from a range to an output iterator
///        if they satisfy a predicate.
///
/// \param first The beginning of the input range.
/// \param last The end of the input range.
/// \param out_first The beginning of the output range.
/// \param transform_op The unary operation to apply to matching elements.
/// \param pred The unary predicate to test elements against.
/// \return An iterator to the end of the new output range.
template <std::input_iterator I,
          std::sentinel_for<I> S,
          typename O,
          typename Func,
          typename Pred>
  requires CTransformIfAbleFromIterators<I, O, Func, Pred>
constexpr O transform_if(
    I first, S last, O out_first, Func transform_op, Pred pred) {
  for (; first != last; ++first) {
    if (std::invoke(pred, *first)) {
      *out_first = std::invoke(transform_op, *first);
      ++out_first;
    }
  }
  return out_first;
}

/// \brief Concept for a valid transform_if operation using a range.
///
/// \details Checks if a predicate and a transform function can be applied
///          to a range's element, and if the transform's result
///          can be written to an output iterator.
///
/// \tparam R The input range type.
/// \tparam O The output iterator type.
/// \tparam Func The transformation function type.
/// \tparam Pred The predicate function type.
template <typename R, typename O, typename Func, typename Pred>
concept CTransformIfAbleFromRange =
    // 'Pred' must be a predicate callable with the range's element type
    std::predicate<Pred, std::ranges::range_reference_t<R>> &&
    // 'Func' must be invocable with the range's element type
    std::invocable<Func, std::ranges::range_reference_t<R>> &&
    // 'O' must be an output iterator that can accept the *result*
    // of the transformation function
    std::output_iterator<
        O,
        std::invoke_result_t<Func, std::ranges::range_reference_t<R>>>;

/// \brief Range-based overload for TransformIf.
///
/// \param range The input range to process.
/// \param out_first The beginning of the output range.
/// \param transform_op The unary operation to apply to matching elements.
/// \param pred The unary predicate to test elements against.
/// \return An iterator to the end of the new output range.
template <std::ranges::input_range R, typename O, typename Func, typename Pred>
  requires CTransformIfAbleFromRange<R, O, Func, Pred>
constexpr O transform_if(R&& range, O out_first, Func transform_op, Pred pred) {
  return transform_if(std::ranges::begin(range),
                      std::ranges::end(range),
                      std::move(out_first),
                      std::move(transform_op),
                      std::move(pred));
}

}  // namespace maia
