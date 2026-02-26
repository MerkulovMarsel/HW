#pragma once
#include "exception.h"
#include "storage.h"
#include "traits.h"
#include "utils.h"

#include <array>
#include <functional>

namespace ct {
template <typename... Ts>
class Variant {
public:
  constexpr Variant() noexcept(std::is_nothrow_default_constructible_v<detail::get_i_t<0, Ts...>>)
    requires (std::is_default_constructible_v<detail::get_i_t<0, Ts...>>)
      : storage(in_place_index<0>)
      , index_(0) {}

  Variant(const Variant&)
    requires detail::all_trivially_copy_ctor<Ts...>
  = default;
  Variant(Variant&&)
    requires detail::all_trivially_move_ctor<Ts...>
  = default;
  Variant& operator=(const Variant&)
    requires detail::all_trivially_copy_assign<Ts...>
  = default;
  Variant& operator=(Variant&&)
    requires (detail::all_trivially_move_assign<Ts...>)
  = default;
  ~Variant()
    requires detail::all_trivially_destructible<Ts...>
  = default;

  constexpr Variant(const Variant& other) noexcept(detail::all_nothrow_copy_ctor<Ts...>)
    requires detail::all_difficult_copy_ctor<Ts...>
      : index_(variant_npos) {
    if (other.valueless_by_exception()) {
      reset();
      return;
    }
    detail::special_index_visit<void>(
        [this]<size_t I, typename T>(T&& alternative) -> void {
          std::construct_at(&storage, in_place_index<I>, std::forward<T>(alternative));
        },
        other
    );
    index_ = other.index();
  }

  constexpr Variant(Variant&& other) noexcept(detail::all_nothrow_move_ctor<Ts...>)
    requires detail::all_difficult_move_ctor<Ts...>
      : index_(variant_npos) {
    if (other.valueless_by_exception()) {
      reset();
      return;
    }
    detail::special_index_visit<void>(
        [this]<size_t I>(auto&& alternative) -> void {
          std::construct_at(&storage, in_place_index<I>, std::forward<decltype(alternative)>(alternative));
        },
        std::move(other)
    );
    index_ = other.index();
  }

  constexpr Variant& operator=(const Variant& other)
    requires detail::all_difficult_copy_assign<Ts...>
  {
    detail::special_index_visit<void>(
        [this, &other]<size_t I, typename T>(const T& alternative) -> void {
          if (!(this == &other || (valueless_by_exception() && other.valueless_by_exception()))) {
            if (other.valueless_by_exception() && !valueless_by_exception()) {
              reset();
            } else if (valueless_by_exception()) {
              std::construct_at(this, other);
            } else if (index_ == other.index()) {
              get<I>() = alternative;
            } else if constexpr (std::is_nothrow_copy_constructible_v<T> || !std::is_nothrow_move_constructible_v<T>) {
              reset();
              std::construct_at(this, other);
            } else {
              this->operator=(Variant(other));
            }
          }
        },
        other
    );
    return *this;
  }

  constexpr Variant&
  operator=(Variant&& other) noexcept(detail::all_nothrow_move_assign<Ts...> && detail::all_nothrow_move_ctor<Ts...>)
    requires detail::all_difficult_move_assign<Ts...>
  {
    detail::special_index_visit<void>(
        [this, &other]<size_t I, typename T>(T&& alternative) -> void {
          if (!(this == &other || (valueless_by_exception() && other.valueless_by_exception()))) {
            if (other.valueless_by_exception() && !valueless_by_exception()) {
              reset();
            } else if (valueless_by_exception()) {
              std::construct_at(this, std::move(other));
            } else if (index_ == other.index()) {
              get<I>() = std::move(alternative);
            } else {
              reset();
              std::construct_at(this, std::move(other));
            }
          }
        },
        std::move(other)
    );
    return *this;
  }

  constexpr ~Variant() noexcept(detail::all_nothrow_destructible<Ts...>)
    requires (detail::all_difficult_destructible<Ts...>)
  {
    destroy();
  }

  template <typename T>
    requires (sizeof...(Ts) > 0 && !std::is_same_v<std::decay_t<T>, Variant> &&
              !detail::is_in_place_tag<std::decay_t<T>> && detail::can_select<T, Ts...> &&
              std::is_constructible_v<detail::selected_type<T, Ts...>, T &&>)
  constexpr Variant(T&& value) noexcept(std::is_nothrow_constructible_v<detail::selected_type<T, Ts...>, T&&>)
      : storage(in_place_index<detail::get_index<detail::selected_type<T, Ts...>, Ts...>>, std::forward<T>(value))
      , index_(detail::get_index<detail::selected_type<T, Ts...>, Ts...>) {}

  template <typename T, typename... Args>
    requires (detail::is_in<T, Ts...> && !detail::has_duplicate<T, Ts...> && std::is_constructible_v<T, Args...>)
  constexpr explicit Variant(InPlaceType<T>, Args&&... args)
      : Variant(in_place_index<detail::get_index<T, Ts...>>, std::forward<Args>(args)...) {}

  template <typename T, class U, typename... Args>
    requires (
        detail::is_in<T, Ts...> && !detail::has_duplicate<T, Ts...> &&
        std::is_constructible_v<T, std::initializer_list<U>, Args...>
    )
  constexpr explicit Variant(InPlaceType<T>, std::initializer_list<U> initializer_list, Args&&... args)
      : Variant(in_place_index<detail::get_index<T, Ts...>>, initializer_list, std::forward<Args>(args)...) {}

  template <size_t N, typename... Args>
    requires (sizeof...(Ts) > N && std::is_constructible_v<detail::get_i_t<N, Ts...>, Args...>)
  constexpr explicit Variant(InPlaceIndex<N> index, Args&&... args)
      : storage(index, std::forward<Args>(args)...)
      , index_(N) {}

  template <size_t N, typename U, typename... Args>
    requires (sizeof...(Ts) > N && std::is_constructible_v<detail::get_i_t<N, Ts...>, Args...>)
  constexpr explicit Variant(InPlaceIndex<N> index, std::initializer_list<U> initializer_list, Args&&... args)
      : storage(index, initializer_list, std::forward<Args>(args)...)
      , index_(N) {}

  template <typename T>
    requires (
        detail::can_select<T, Ts...> && std::is_constructible_v<detail::selected_type<T, Ts...>, T> &&
        std::is_assignable_v<detail::selected_type<T, Ts...>&, T>
    )
  constexpr Variant& operator=(T&& value) noexcept(
      std::is_nothrow_assignable_v<detail::selected_type<T, Ts...>&, T> &&
      std::is_nothrow_constructible_v<detail::selected_type<T, Ts...>, T>
  ) {
    using selected_t = detail::selected_type<T, Ts...>;
    static constexpr size_t index_assign = detail::get_index<selected_t, Ts...>;
    if (index() == index_assign) {
      get<index_assign>() = std::forward<T>(value);
    } else if constexpr (std::is_nothrow_constructible_v<selected_t, T&&> ||
                         !std::is_nothrow_move_constructible_v<selected_t>) {
      emplace<index_assign>(std::forward<T>(value));
    } else {
      emplace<index_assign>(selected_t(std::forward<T>(value)));
    }
    return *this;
  }

  constexpr size_t index() const noexcept {
    return index_;
  }

  constexpr bool valueless_by_exception() const noexcept {
    return index() == variant_npos;
  }

  template <typename T, typename... Args>
    requires (!detail::has_duplicate<T, Ts...> && std::is_constructible_v<T, Args...>)
  constexpr T& emplace(Args&&... args) {
    return emplace<detail::get_index<T, Ts...>>(std::forward<Args>(args)...);
  }

  template <typename T, typename U, typename... Args>
    requires (!detail::has_duplicate<T, Ts...> && std::is_constructible_v<T, std::initializer_list<U>&, Args...>)
  constexpr T& emplace(std::initializer_list<U> initializer_list, Args&&... args) {
    return emplace<detail::get_index<T, Ts...>>(initializer_list, std::forward<Args>(args)...);
  }

  template <size_t N, typename... Args>
    requires (sizeof...(Ts) > N && std::is_constructible_v<detail::get_i_t<N, Ts...>, Args...>)
  constexpr auto& emplace(Args&&... args) {
    if (!valueless_by_exception()) {
      reset();
    }
    std::construct_at(&storage, in_place_index<N>, std::forward<Args>(args)...);
    index_ = N;
    return get<N>();
  }

  template <size_t N, typename U, typename... Args>
    requires (
        sizeof...(Ts) > N && std::is_constructible_v<detail::get_i_t<N, Ts...>, std::initializer_list<U>&, Args...>
    )
  constexpr auto& emplace(std::initializer_list<U> initializer_list, Args&&... args) {
    if (!valueless_by_exception()) {
      reset();
    }
    storage.template emplace<N>(initializer_list, std::forward<Args>(args)...);
    index_ = N;
    return get<N>();
  }

  constexpr void swap(Variant& other) noexcept(
      std::is_nothrow_move_constructible_v<Variant> && (std::is_nothrow_swappable_v<Ts> && ...)
  )
    requires (detail::all_move_ctor<Ts...> && (std::is_swappable_v<Ts> && ...))
  {
    if (valueless_by_exception() || other.valueless_by_exception()) {
      if (valueless_by_exception()) {
        if (!other.valueless_by_exception()) {
          other.swap(*this);
        }
        return;
      }
      std::construct_at(&other, std::move(*this));
      reset();
      return;
    }
    if (index() == other.index()) {
      ct::visit<void>(
          []<typename TL, typename TR>(TL&& l, TR&& r) {
            if constexpr (std::is_same_v<TL, TR>) {
              using std::swap;
              swap(l, r);
            }
          },
          *this,
          other
      );
      return;
    }
    Variant tmp = std::move(other);
    other.destroy();
    std::construct_at(&other, std::move(*this));
    destroy();
    std::construct_at(this, std::move(tmp));
  }

  template <detail::Op op, typename... Types>
  friend constexpr bool cmp_impl(const Variant<Types...>& lhs, const Variant<Types...>& rhs);

  template <typename... Types>
  friend constexpr std::common_comparison_category_t<std::compare_three_way_result_t<Types>...>
  operator<=>(const Variant<Types...>& lhs, const Variant<Types...>& rhs);

  template <size_t N, typename Variant>
  friend constexpr decltype(auto) detail::get_impl(Variant&& variant);

private:
  template <size_t N>
  constexpr const detail::get_i_t<N, Ts...>& get() const {
    return ct::get<N>(*this);
  }

  template <size_t N>
  constexpr detail::get_i_t<N, Ts...>& get() {
    return ct::get<N>(*this);
  }

  template <size_t N>
  constexpr std::add_pointer_t<const detail::get_i_t<N, Ts...>> get_if() const noexcept {
    return ct::get_if<N>(*this);
  }

  template <size_t N>
  constexpr std::add_pointer_t<detail::get_i_t<N, Ts...>> get_if() noexcept {
    return ct::get_if<N>(*this);
  }

  constexpr void destroy() {
    if (!valueless_by_exception()) {
      visit([](auto&& alternative) { std::destroy_at(std::addressof(alternative)); }, *this);
    }
  }

  constexpr void reset() {
    destroy();
    index_ = variant_npos;
    std::construct_at(&storage);
  }

private:
  detail::Storage<Ts...> storage;
  size_t index_;
};

} // namespace ct
