#pragma once

#include "optional-base.h"

namespace ct {

struct ConstructKey {};

struct NullOpt {
  explicit constexpr NullOpt(ConstructKey) {}
};

inline constexpr NullOpt nullopt{ConstructKey{}};

struct InPlace {
  explicit constexpr InPlace() {}
};

inline constexpr InPlace in_place{};

template <typename T>
class Optional final : protected detail::MoveAssignBase<T> {
  using Base = detail::MoveAssignBase<T>;

public:
  using ValueType = T;
  using Base::Base;
  using Base::operator=;

  constexpr Optional(NullOpt) noexcept
      : Optional() {}

  template <
      typename U = std::remove_cv_t<T>,
      std::enable_if_t<
          std::is_constructible_v<T, U&&> && !std::is_same_v<std::remove_cvref_t<U>, Optional> &&
              !std::is_same_v<InPlace, std::remove_cvref_t<U>> && std::is_convertible_v<U, T>,
          int> = 0>
  constexpr Optional(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
    emplace(std::forward<U>(value));
  }

  template <
      typename U = std::remove_cv_t<T>,
      std::enable_if_t<
          std::is_constructible_v<T, U&&> && !std::is_same_v<std::remove_cvref_t<U>, Optional> &&
              !std::is_same_v<InPlace, std::remove_cvref_t<U>> && !std::is_convertible_v<U, T>,
          int> = 1>
  explicit constexpr Optional(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
    emplace(std::forward<U>(value));
  }

  template <
      typename U,
      std::enable_if_t<
          std::is_constructible_v<T, const U&> && !std::is_same_v<U, T> && std::is_convertible_v<const U&, T>,
          int> = 0>
  constexpr Optional(const Optional<U>& value) noexcept(std::is_nothrow_constructible_v<T, const U&>) {
    if (value.has_value()) {
      emplace(*value);
    }
  }

  template <
      typename U,
      std::enable_if_t<
          std::is_constructible_v<T, const U&> && !std::is_same_v<U, T> && !std::is_convertible_v<const U&, T>,
          int> = 0>
  explicit constexpr Optional(const Optional<U>& value) noexcept(std::is_nothrow_constructible_v<T, const U&>) {
    if (value.has_value()) {
      emplace(*value);
    }
  }

  template <
      typename U,
      std::enable_if_t<std::is_constructible_v<T, U&&> && !std::is_same_v<U, T> && std::is_convertible_v<U&&, T>, int> =
          0>
  constexpr Optional(Optional<U>&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
    if (value.has_value()) {
      emplace(std::move(*value));
      value.reset();
    }
  }

  template <
      typename U,
      std::enable_if_t<
          std::is_constructible_v<T, U&&> && !std::is_same_v<U, T> && !std::is_convertible_v<U&&, T>,
          int> = 0>
  explicit constexpr Optional(
      Optional<U>&& value
  ) noexcept(std::is_nothrow_constructible_v<T, U&&> && std::is_nothrow_destructible_v<U>) {
    if (value.has_value()) {
      emplace(std::move(*value));
      value.reset();
    }
  }

  template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args&&...>>>
  explicit constexpr Optional(InPlace, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>) {
    std::construct_at(std::addressof(this->value_), std::forward<Args>(args)...);
    Base::valid_ = true;
  }

  constexpr Optional& operator=(NullOpt) noexcept {
    reset();
    return *this;
  }

  template <
      typename U = T,
      typename = std::enable_if_t<
          std::is_constructible_v<T, U&&> && std::is_assignable_v<T&, U&&> &&
          !std::is_same_v<std::remove_cvref_t<U>, Optional>>>
  constexpr Optional&
  operator=(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&> && std::is_nothrow_assignable_v<T&, U&&>) {
    if (has_value()) {
      Base::value_ = std::forward<U>(value);
    } else {
      std::construct_at(std::addressof(this->value_), std::forward<U>(value));
    }
    Base::valid_ = true;
    return *this;
  }

  template <
      typename O,
      typename U = std::remove_cvref_t<O>,
      typename = std::enable_if_t<
          std::is_same_v<U, Optional<typename U::value_type>> &&
          std::is_constructible_v<T, decltype(*std::declval<O&&>())> &&
          std::is_assignable_v<T&, decltype(*std::declval<O&&>())> &&
          !std::is_same_v<T, std::remove_cvref_t<typename U::value_type>>>>
  constexpr Optional& operator=(O&& other) noexcept(
      std::is_nothrow_constructible_v<T, decltype(*std::declval<O&&>())> &&
      std::is_nothrow_assignable_v<T&, decltype(*std::declval<O&&>())> && std::is_nothrow_destructible_v<T>
  ) {
    if (other.has_value()) {
      if (has_value()) {
        Base::value_ = std::forward<decltype(*other)>(*other);
      } else {
        std::construct_at(&Base::value_, std::forward<decltype(*other)>(*other));
      }
      Base::valid_ = true;
    } else {
      reset();
    }
    return *this;
  }

  constexpr Optional& operator=(std::initializer_list<std::nullptr_t>) noexcept(
      std::is_nothrow_constructible_v<T> && std::is_nothrow_assignable_v<T&, T&&>
  ) {
    reset();
    return *this;
  }

  constexpr bool has_value() const noexcept {
    return Base::valid_;
  }

  constexpr explicit operator bool() const noexcept {
    return has_value();
  }

  constexpr T& operator*() & noexcept {
    return Base::value_;
  }

  constexpr const T& operator*() const& noexcept {
    return Base::value_;
  }

  constexpr T&& operator*() && noexcept {
    return std::move(Base::value_);
  }

  constexpr const T&& operator*() const&& noexcept {
    return std::move(Base::value_);
  }

  constexpr T* operator->() noexcept {
    return std::addressof(this->value_);
  }

  constexpr const T* operator->() const noexcept {
    return std::addressof(this->value_);
  }

  template <typename... Args>
  constexpr std::enable_if_t<std::is_constructible_v<T, Args...>, T&>
  emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...> && std::is_nothrow_destructible_v<T>) {
    reset();
    std::construct_at(std::addressof(this->value_), std::forward<Args>(args)...);
    Base::valid_ = true;
    return Base::value_;
  }

  constexpr void reset() noexcept(std::is_nothrow_destructible_v<T>) {
    if (Base::valid_) {
      Base::value_.~T();
      Base::valid_ = false;
    }
  }
};

template <typename T>
constexpr void swap(
    Optional<T>& lhs,
    Optional<T>& rhs,
    std::enable_if_t<std::is_swappable_v<T> && std::is_move_constructible_v<T>, bool> = true
) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>) {
  if (!lhs.has_value() && !rhs.has_value()) {
    return;
  }
  if (!lhs.has_value()) {
    lhs.emplace(std::move(*rhs));
    rhs.reset();
  } else if (!rhs.has_value()) {
    rhs.emplace(std::move(*lhs));
    lhs.reset();
  } else {
    using std::swap;
    swap(*lhs, *rhs);
  }
}

template <typename T>
constexpr void swap(
    Optional<T>&,
    Optional<T>&,
    std::enable_if_t<!std::is_swappable_v<T> || !std::is_move_constructible_v<T>, int> = 0
) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>) = delete;

template <typename T>
constexpr bool operator==(const Optional<T>& lhs, const Optional<T>& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }
  if (!lhs.has_value()) {
    return true;
  }
  return *lhs == *rhs;
}

template <typename T>
constexpr bool operator!=(const Optional<T>& lhs, const Optional<T>& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return true;
  }
  if (!lhs.has_value()) {
    return false;
  }
  return *lhs != *rhs;
}

template <typename T>
constexpr bool operator<(const Optional<T>& lhs, const Optional<T>& rhs) {
  if (!rhs.has_value()) {
    return false;
  }
  if (!lhs.has_value()) {
    return true;
  }
  return *lhs < *rhs;
}

template <typename T>
constexpr bool operator<=(const Optional<T>& lhs, const Optional<T>& rhs) {
  if (!lhs.has_value()) {
    return true;
  }
  if (!rhs.has_value()) {
    return false;
  }
  return *lhs <= *rhs;
}

template <typename T>
constexpr bool operator>(const Optional<T>& lhs, const Optional<T>& rhs) {
  if (!lhs.has_value()) {
    return false;
  }
  if (!rhs.has_value()) {
    return true;
  }
  return *lhs > *rhs;
}

template <typename T>
constexpr bool operator>=(const Optional<T>& lhs, const Optional<T>& rhs) {
  if (!rhs.has_value()) {
    return true;
  }
  if (!lhs.has_value()) {
    return false;
  }
  return *lhs >= *rhs;
}

template <class T>
constexpr std::compare_three_way_result_t<T> operator<=>(const Optional<T>& lhs, const Optional<T>& rhs) {
  return lhs && rhs ? *lhs <=> *rhs : lhs.has_value() <=> rhs.has_value();
}

template <typename T>
Optional(T) -> Optional<T>;

} // namespace ct
