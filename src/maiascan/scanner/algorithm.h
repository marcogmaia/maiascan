// Copyright (c) Maia

#pragma once

namespace scanner {

template <typename InputIterator,
          typename OutputIterator,
          typename UnaryOperator,
          typename Pred>
OutputIterator transform_if(InputIterator first,
                            InputIterator last,
                            OutputIterator out_first,
                            UnaryOperator transform_op,
                            Pred pred) {
  for (; first != last; ++first) {
    if (pred(*first)) {
      *out_first = transform_op(*first);
      ++out_first;
    }
  }
  return out_first;
}

}  // namespace scanner
