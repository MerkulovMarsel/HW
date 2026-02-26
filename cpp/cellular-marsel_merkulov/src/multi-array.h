#pragma once

#include <array>
#include <vector>

namespace ct::detail {
template <typename State, size_t D>
struct MultiArr {
  explicit MultiArr(const std::array<std::size_t, D>& extends)
      : sizes(extends) {
    size_t size = 1;
    for (size_t i = 0; i < D; ++i) {
      size *= extends[i];
    }
    data.resize(size);
  }

  size_t to_primary(std::array<size_t, D> indices) const {
    size_t index = 0;
    size_t cur_index = 1;
    for (size_t i = D; i-- > 0;) {
      index += indices[i] * cur_index;
      cur_index *= sizes[i];
    }
    return index;
  }

  template <typename... Indices>
    requires (sizeof...(Indices) == D && (std::convertible_to<Indices, size_t> && ...))
  size_t to_primary(Indices... indices) const {
    return to_primary(std::array<size_t, D>{static_cast<size_t>(indices)...});
  }

  std::array<std::size_t, D> from_primary(std::size_t index) const {
    std::array<size_t, D> indices;
    for (size_t i = D; i-- > 0;) {
      indices[i] = index % sizes[i];
      index /= sizes[i];
    }
    return indices;
  }

  template <typename... Indices>
    requires (sizeof...(Indices) == D)
  State& operator[](Indices... indices) {
    return data[to_primary(indices...)];
  }

  State& operator[](const std::array<size_t, D>& indices) {
    return data[to_primary(indices)];
  }

  std::vector<State> data;
  std::array<size_t, D> sizes;
};

} // namespace ct::detail
