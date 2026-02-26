#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace ct {

template <size_t N>
struct InPlaceIndex {};

template <size_t N>
static constexpr InPlaceIndex<N> in_place_index;

template <typename T>
struct InPlaceType {};

template <typename T>
static constexpr InPlaceType<T> in_place_type;

static constexpr size_t variant_npos = std::numeric_limits<size_t>::max();

namespace detail {

template <typename T>
static constexpr bool is_in_place_index = false;

template <size_t N>
static constexpr bool is_in_place_index<InPlaceIndex<N>> = true;

template <typename T>
static constexpr bool is_in_place_type = false;

template <typename T>
static constexpr bool is_in_place_type<InPlaceType<T>> = true;

template <typename T>
static constexpr bool is_in_place_tag = is_in_place_index<T> || is_in_place_type<T>;

template <size_t N, typename First, typename... Rest>
struct get_i {
  using type = typename get_i<N - 1, Rest...>::type;
};

template <typename First, typename... Rest>
struct get_i<0, First, Rest...> {
  using type = First;
};

template <size_t N, typename... Types>
using get_i_t = typename get_i<N, Types...>::type;

template <typename T, typename... Types>
struct get_t;

template <typename T, typename First, typename... Rest>
struct get_t<T, First, Rest...> {
  using type = typename get_t<T, Rest...>::type;
};

template <typename First, typename... Rest>
struct get_t<First, First, Rest...> {
  using type = First;
};

template <typename T, typename... Types>
using get_t_t = typename get_t<T, Types...>::type;

template <typename T, typename... Rest>
static constexpr size_t get_index_impl = variant_npos;

template <typename T, typename First, typename... Rest>
static constexpr size_t get_index_impl<T, First, Rest...> = get_index_impl<T, Rest...> + 1;

template <typename T, typename... Rest>
static constexpr size_t get_index_impl<T, T, Rest...> = 0;

template <typename T, typename... Types>
static constexpr bool is_in = (std::is_same_v<T, Types> || ...);

template <typename T, typename... Types>
static constexpr size_t get_index = is_in<T, Types...> ? get_index_impl<T, Types...> : variant_npos;

template <typename T>
struct Arr {
  T array[1];
};

template <typename From, typename To>
concept non_narrowing = requires { Arr<To>{{std::declval<From>()}}; };

template <typename T, typename... Ts>
static constexpr bool contains = (std::is_same_v<T, Ts> || ...);

template <typename From, typename... Types>
struct overload_chain;

template <typename From, typename T, typename... Rest>
  requires (non_narrowing<From, T> && !contains<T, Rest...>)
struct overload_chain<From, T, Rest...> : overload_chain<From, Rest...> {
  static decltype(std::declval<T>()) F(T);

  using overload_chain<From, Rest...>::F;
};

template <typename From, typename T, typename... Rest>
  requires (!non_narrowing<From, T> || contains<T, Rest...>)
struct overload_chain<From, T, Rest...> : overload_chain<From, Rest...> {
  using overload_chain<From, Rest...>::F;
};

template <typename From>
struct overload_chain<From> {
  static void F();
};

template <typename T, typename... Types>
using selected_type = std::remove_reference_t<decltype(overload_chain<T, Types...>::F(std::declval<T>()))>;

template <typename T, typename... Types>
concept has_duplicate = (std::is_same_v<T, Types> + ...) > 1;

template <typename T, typename... Types>
concept can_select = !has_duplicate<T, Types...> && requires { typename selected_type<T, Types...>; };

template <typename... Types>
concept all_default_ctor = (std::is_default_constructible_v<Types> && ...);

template <typename... Types>
concept all_trivially_default_ctor =
    all_default_ctor<Types...> && (std::is_trivially_default_constructible_v<Types> && ...);

template <typename... Types>
concept all_difficult_default_ctor = all_default_ctor<Types...> && !all_trivially_default_ctor<Types...>;

template <typename... Types>
concept all_nothrow_default_ctor =
    all_default_ctor<Types...> && (std::is_nothrow_default_constructible_v<Types> && ...);

template <typename... Types>
concept all_copy_ctor = (std::is_copy_constructible_v<Types> && ...);

template <typename... Types>
concept all_trivially_copy_ctor = all_copy_ctor<Types...> && (std::is_trivially_copy_constructible_v<Types> && ...);

template <typename... Types>
concept all_difficult_copy_ctor = all_copy_ctor<Types...> && !all_trivially_copy_ctor<Types...>;

template <typename... Types>
concept all_nothrow_copy_ctor = all_copy_ctor<Types...> && (std::is_nothrow_copy_constructible_v<Types> && ...);

template <typename... Types>
concept all_move_ctor = (std::is_move_constructible_v<Types> && ...);

template <typename... Types>
concept all_trivially_move_ctor = all_move_ctor<Types...> && (std::is_trivially_move_constructible_v<Types> && ...);

template <typename... Types>
concept all_difficult_move_ctor = all_move_ctor<Types...> && !all_trivially_move_ctor<Types...>;

template <typename... Types>
concept all_nothrow_move_ctor = all_move_ctor<Types...> && (std::is_nothrow_move_constructible_v<Types> && ...);

template <typename... Types>
concept all_copy_assign = all_copy_ctor<Types...> && (std::is_copy_assignable_v<Types> && ...);

template <typename... Types>
concept all_trivially_copy_assign = all_copy_assign<Types...> && all_trivially_copy_ctor<Types...> &&
                                    (std::is_trivially_copy_assignable_v<Types> && ...);

template <typename... Types>
concept all_difficult_copy_assign = all_copy_assign<Types...> && !all_trivially_copy_assign<Types...>;

template <typename... Types>
concept all_nothrow_copy_assign = all_copy_assign<Types...> && (std::is_nothrow_copy_assignable_v<Types> && ...);

template <typename... Types>
concept all_move_assign = all_move_ctor<Types...> && (std::is_move_assignable_v<Types> && ...);

template <typename... Types>
concept all_trivially_move_assign = all_move_assign<Types...> && all_trivially_move_ctor<Types...> &&
                                    (std::is_trivially_move_assignable_v<Types> && ...);

template <typename... Types>
concept all_difficult_move_assign = all_move_assign<Types...> && !all_trivially_move_assign<Types...>;

template <typename... Types>
concept all_nothrow_move_assign = all_move_assign<Types...> && (std::is_nothrow_move_assignable_v<Types> && ...);

template <typename... Types>
concept all_destructible = (std::is_destructible_v<Types> && ...);

template <typename... Types>
concept all_trivially_destructible = all_destructible<Types...> && (std::is_trivially_destructible_v<Types> && ...);

template <typename... Types>
concept all_difficult_destructible = all_destructible<Types...> && !all_trivially_destructible<Types...>;

template <typename... Types>
concept all_nothrow_destructible = all_destructible<Types...> && (std::is_nothrow_destructible_v<Types> && ...);

} // namespace detail
} // namespace ct
