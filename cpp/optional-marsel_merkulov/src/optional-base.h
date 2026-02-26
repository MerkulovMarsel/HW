#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace ct::detail {

enum class OriginalType {
  Normal,
  Trivial,
  Deleted
};

enum class OperationType {
  CopyCtor,
  MoveCtor,
  CopyAssign,
  MoveAssign,
  Dtor
};

template <typename T>
consteval OriginalType get_operation_type(OperationType type) {
  constexpr auto check_operation = [](bool is_available, bool is_trivial) {
    if (!is_available) {
      return OriginalType::Deleted;
    }
    return is_trivial ? OriginalType::Trivial : OriginalType::Normal;
  };

  switch (type) {
  case OperationType::CopyCtor:
    return check_operation(std::is_copy_constructible_v<T>, std::is_trivially_copy_constructible_v<T>);
  case OperationType::MoveCtor:
    return check_operation(std::is_move_constructible_v<T>, std::is_trivially_move_constructible_v<T>);
  case OperationType::CopyAssign:
    return check_operation(
        std::is_copy_assignable_v<T> && std::is_copy_constructible_v<T>,
        std::is_trivially_copy_assignable_v<T> && std::is_trivially_copy_constructible_v<T>
    );
  case OperationType::MoveAssign:
    return check_operation(
        std::is_move_assignable_v<T> && std::is_move_constructible_v<T>,
        std::is_trivially_move_assignable_v<T> && std::is_trivially_move_constructible_v<T>
    );
  case OperationType::Dtor:
    return check_operation(std::is_destructible_v<T>, std::is_trivially_destructible_v<T>);
  }
  std::unreachable();
}

template <typename T, OriginalType original_type = get_operation_type<T>(OperationType::Dtor)>
struct OptionalBase;

template <typename T>
struct OptionalBase<T, OriginalType::Deleted> {
  union {
    T value_;
  };

  bool valid_;

  constexpr OptionalBase() noexcept {
    valid_ = false;
  }

  constexpr ~OptionalBase() = delete;

  OptionalBase& operator=(const OptionalBase&) = default;
  OptionalBase(OptionalBase&&) = default;
  OptionalBase(const OptionalBase&) = default;
  OptionalBase& operator=(OptionalBase&&) = default;
};

template <typename T>
struct OptionalBase<T, OriginalType::Trivial> {
  union {
    T value_;
  };

  bool valid_;

  constexpr OptionalBase() noexcept {
    valid_ = false;
  }

  constexpr ~OptionalBase() = default;

  OptionalBase& operator=(const OptionalBase&) = default;
  OptionalBase(OptionalBase&&) = default;
  OptionalBase(const OptionalBase&) = default;
  OptionalBase& operator=(OptionalBase&&) = default;
};

template <typename T>
struct OptionalBase<T, OriginalType::Normal> {
  union {
    T value_;
  };

  bool valid_;

  constexpr OptionalBase() noexcept {
    valid_ = false;
  }

  constexpr ~OptionalBase() noexcept(std::is_nothrow_destructible_v<T>) {
    if (valid_) {
      std::destroy_at(&value_);
      valid_ = false;
    }
  }

  OptionalBase& operator=(const OptionalBase&) = default;
  OptionalBase(OptionalBase&&) = default;
  OptionalBase(const OptionalBase&) = default;
  OptionalBase& operator=(OptionalBase&&) = default;
};

template <typename T, OriginalType original_type = get_operation_type<T>(OperationType::CopyCtor)>
struct CopyCtorBase;

template <typename T>
struct CopyCtorBase<T, OriginalType::Deleted> : OptionalBase<T> {
  using OptionalBase<T>::OptionalBase;
  CopyCtorBase(const CopyCtorBase&) = delete;

  CopyCtorBase() = default;
  CopyCtorBase(CopyCtorBase&&) = default;
  CopyCtorBase& operator=(const CopyCtorBase&) = default;
  CopyCtorBase& operator=(CopyCtorBase&&) = default;
  ~CopyCtorBase() = default;
};

template <typename T>
struct CopyCtorBase<T, OriginalType::Trivial> : OptionalBase<T> {
  using OptionalBase<T>::OptionalBase;
};

template <typename T>
struct CopyCtorBase<T, OriginalType::Normal> : OptionalBase<T> {
  using OptionalBase<T>::OptionalBase;
  using Base = OptionalBase<T>;

  constexpr CopyCtorBase(const CopyCtorBase& other) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    if (other.Base::valid_) {
      std::construct_at(std::addressof(this->value_), other.Base::value_);
      Base::valid_ = true;
    }
  }

  CopyCtorBase() = default;
  CopyCtorBase(CopyCtorBase&&) = default;
  CopyCtorBase& operator=(const CopyCtorBase&) = default;
  CopyCtorBase& operator=(CopyCtorBase&&) = default;
  ~CopyCtorBase() = default;
};

template <typename T, OriginalType original_type = get_operation_type<T>(OperationType::MoveCtor)>
struct MoveCtorBase;

template <typename T>
struct MoveCtorBase<T, OriginalType::Deleted> : CopyCtorBase<T> {
  using CopyCtorBase<T>::CopyCtorBase;

  MoveCtorBase() = default;
  MoveCtorBase& operator=(const MoveCtorBase&) = default;
  MoveCtorBase(const MoveCtorBase& other) = default;
  MoveCtorBase& operator=(MoveCtorBase&&) = default;
  ~MoveCtorBase() = default;
};

template <typename T>
struct MoveCtorBase<T, OriginalType::Trivial> : CopyCtorBase<T> {
  using CopyCtorBase<T>::CopyCtorBase;
};

template <typename T>
struct MoveCtorBase<T, OriginalType::Normal> : CopyCtorBase<T> {
  using CopyCtorBase<T>::CopyCtorBase;
  using Base = CopyCtorBase<T>;

  constexpr MoveCtorBase(
      MoveCtorBase&& other
  ) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>) {
    if (other.Base::valid_) {
      std::construct_at(std::addressof(this->value_), std::move(other.Base::value_));
      Base::valid_ = true;
    }
  }

  MoveCtorBase() = default;
  MoveCtorBase& operator=(const MoveCtorBase&) = default;
  MoveCtorBase(const MoveCtorBase& other) = default;
  MoveCtorBase& operator=(MoveCtorBase&&) = default;
  ~MoveCtorBase() = default;
};

template <typename T, OriginalType original_type = get_operation_type<T>(OperationType::CopyAssign)>
struct CopyAssignBase;

template <typename T>
struct CopyAssignBase<T, OriginalType::Deleted> : MoveCtorBase<T> {
  using MoveCtorBase<T>::MoveCtorBase;
  CopyAssignBase& operator=(const CopyAssignBase&) = delete;

  CopyAssignBase() = default;
  CopyAssignBase(const CopyAssignBase& other) = default;
  CopyAssignBase(CopyAssignBase&&) = default;
  CopyAssignBase& operator=(CopyAssignBase&&) = default;
  ~CopyAssignBase() = default;
};

template <typename T>
struct CopyAssignBase<T, OriginalType::Trivial> : MoveCtorBase<T> {
  using MoveCtorBase<T>::MoveCtorBase;
  using Base = MoveCtorBase<T>;
};

template <typename T>
struct CopyAssignBase<T, OriginalType::Normal> : MoveCtorBase<T> {
  using MoveCtorBase<T>::MoveCtorBase;
  using Base = MoveCtorBase<T>;

  constexpr CopyAssignBase& operator=(const CopyAssignBase& other) noexcept(
      std::is_nothrow_copy_assignable_v<T> && std::is_nothrow_copy_constructible_v<T> &&
      std::is_nothrow_destructible_v<T>
  ) {
    if (this == &other) {
      return *this;
    }
    if (other.Base::valid_) {
      if (!Base::valid_) {
        std::construct_at(std::addressof(this->value_), other.Base::value_);
        Base::valid_ = true;
      } else {
        Base::value_ = other.Base::value_;
      }
    } else if (Base::valid_) {
      Base::value_.~T();
      Base::valid_ = false;
    }
    return *this;
  }

  CopyAssignBase() = default;
  CopyAssignBase(const CopyAssignBase& other) = default;
  CopyAssignBase(CopyAssignBase&&) = default;
  CopyAssignBase& operator=(CopyAssignBase&&) = default;
  ~CopyAssignBase() = default;
};

template <typename T, OriginalType original_type = get_operation_type<T>(OperationType::MoveAssign)>
struct MoveAssignBase;

template <typename T>
struct MoveAssignBase<T, OriginalType::Deleted> : CopyAssignBase<T> {
  using CopyAssignBase<T>::CopyAssignBase;
  using CopyAssignBase<T>::operator=;
  MoveAssignBase& operator=(MoveAssignBase&&) = delete;

  MoveAssignBase() = default;
  MoveAssignBase(MoveAssignBase&&) = default;
  MoveAssignBase& operator=(const MoveAssignBase&) = default;
  MoveAssignBase(const MoveAssignBase& other) = default;
  ~MoveAssignBase() = default;
};

template <typename T>
struct MoveAssignBase<T, OriginalType::Trivial> : CopyAssignBase<T> {
  using CopyAssignBase<T>::CopyAssignBase;
  using CopyAssignBase<T>::operator=;
};

template <typename T>
struct MoveAssignBase<T, OriginalType::Normal> : CopyAssignBase<T> {
  using CopyAssignBase<T>::CopyAssignBase;
  using CopyAssignBase<T>::operator=;
  using Base = CopyAssignBase<T>;

  constexpr MoveAssignBase& operator=(MoveAssignBase&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
      std::is_nothrow_destructible_v<T>
  ) {
    if (this != &other) {
      if (other.Base::valid_) {
        if (!Base::valid_) {
          std::construct_at(std::addressof(this->value_), std::move(other.Base::value_));
          Base::valid_ = true;
        } else {
          Base::value_ = std::move(other.Base::value_);
        }
      } else if (Base::valid_) {
        Base::value_.~T();
        Base::valid_ = false;
      }
    }
    return *this;
  }

  MoveAssignBase() = default;
  MoveAssignBase(MoveAssignBase&&) = default;
  MoveAssignBase& operator=(const MoveAssignBase&) = default;
  MoveAssignBase(const MoveAssignBase& other) = default;
  ~MoveAssignBase() = default;
};

} // namespace ct::detail
