#pragma once

#include "traits.h"

#include <memory>

namespace ct::detail {
template <typename... Types>
union Storage {
  constexpr Storage() {}

  template <size_t N, typename... Args>
  explicit constexpr Storage(InPlaceIndex<N>, Args&&...) {}

  explicit Storage(size_t) {}

  explicit constexpr Storage(const Storage&, size_t) {}

  explicit constexpr Storage(Storage&&, size_t) {}
};

template <typename First, typename... Rest>
union Storage<First, Rest...> {
  constexpr Storage()
      : rest() {}

  Storage(const Storage&)
    requires (all_trivially_copy_ctor<First, Rest...>)
  = default;
  Storage& operator=(const Storage&)
    requires (all_trivially_copy_assign<First, Rest...>)
  = default;
  Storage(Storage&&)
    requires (all_trivially_move_ctor<First, Rest...>)
  = default;
  Storage& operator=(Storage&&)
    requires (all_trivially_move_assign<First, Rest...>)
  = default;

  template <size_t N, typename... Args>
  explicit constexpr Storage(InPlaceIndex<N>, Args&&... args)
      : rest(in_place_index<N - 1>, std::forward<Args>(args)...) {}

  template <typename... Args>
  explicit constexpr Storage(InPlaceIndex<0>, Args&&... args)
      : first(std::forward<Args>(args)...) {}

  ~Storage()
    requires all_trivially_destructible<First, Rest...>
  = default;

  constexpr ~Storage()
    requires all_difficult_destructible<First, Rest...>
  {}

  template <size_t N, typename... Args>
  constexpr void emplace(Args&&... args) {
    if constexpr (N == 0) {
      std::construct_at(std::addressof(first), std::forward<Args>(args)...);
    } else {
      rest.template emplace<N - 1, Args...>(std::forward<Args>(args)...);
    }
  }

  template <size_t N, typename Storage>
  static constexpr decltype(auto) get_impl(Storage&& storage) noexcept {
    if constexpr (N == 0) {
      return (std::forward<Storage>(storage).first);
    } else {
      return get_impl<N - 1>(std::forward<Storage>(storage).rest);
    }
  }

  template <size_t N>
  constexpr const auto& get() const noexcept {
    return get_impl<N>(*this);
  }

  template <size_t N>
  constexpr auto& get() noexcept {
    return get_impl<N>(*this);
  }

  First first;
  Storage<Rest...> rest;
};
} // namespace ct::detail
