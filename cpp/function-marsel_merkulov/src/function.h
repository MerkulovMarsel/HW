#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace ct {

class BadFunctionCall : public std::exception {
  const char* what() const noexcept override {
    return "Call empty Functional";
  }
};

namespace impl {
static constexpr size_t SMALL_SIZE = sizeof(void*);
static constexpr size_t SMALL_ALIGN = alignof(void*);

template <typename F>
inline constexpr bool is_small =
    (sizeof(F) <= SMALL_SIZE) && (alignof(F) <= SMALL_ALIGN) && std::is_nothrow_move_constructible_v<F>;

template <typename R, typename... Args>
struct EmptyImpl {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;

  static void destroy(Storage) {}

  static void copy(ConstStorage, Storage) {}

  static void move(Storage, Storage) noexcept {}

  static R invoke(Storage, Args&&...) {
    throw BadFunctionCall{};
  }
};

template <bool is_small, typename F, typename R, typename... Args>
struct Impl;

template <typename F, typename R, typename... Args>
struct Impl<true, F, R, Args...> {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;

  static void destroy(Storage storage) {
    get_func(storage)->~F();
  }

  static void copy(ConstStorage src, Storage dst) {
    new (dst) F(*get_func(src));
  }

  static void move(Storage src, Storage dst) noexcept {
    F* src_obj = get_func(src);
    new (dst) F(std::move(*src_obj));
    src_obj->~F();
  }

  static R invoke(Storage storage, Args&&... args) {
    return (*get_func(storage))(std::forward<Args>(args)...);
  }

private:
  static F* get_func(Storage& storage) {
    return std::launder(reinterpret_cast<F*>(storage));
  }

  static const F* get_func(ConstStorage& storage) {
    return std::launder(reinterpret_cast<const F*>(storage));
  }
};

template <typename F, typename R, typename... Args>
struct Impl<false, F, R, Args...> {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;

  static void destroy(Storage storage) {
    delete get_func(storage);
  }

  static void copy(ConstStorage src, Storage dst) {
    new (dst) F*(new F(*get_func(src)));
  }

  static void move(Storage src, Storage dst) noexcept {
    F* ptr = get_func(src);
    new (dst) F*(ptr);
  }

  static R invoke(Storage storage, Args&&... args) {
    return (*get_func(storage))(std::forward<Args>(args)...);
  }

private:
  static F* get_func(Storage& storage) {
    return *std::launder(reinterpret_cast<F**>(storage));
  }

  static const F* get_func(ConstStorage& storage) {
    return *std::launder(reinterpret_cast<F* const*>(storage));
  }
};

template <typename R, typename... Args>
struct Descriptors;

template <typename R, typename... Args>
struct Descriptors<R(Args...)> {
  using destroy_t = void (*)(std::byte*);
  using call_t = R (*)(std::byte*, Args&&...);
  using copy_t = void (*)(const std::byte*, std::byte*);
  using move_t = void (*)(std::byte*, std::byte*) noexcept;
  using is_t = void (*)();

  destroy_t destroy = nullptr;
  copy_t copy = nullptr;
  call_t call = nullptr;
  move_t move = nullptr;
};

template <typename R, typename... Args>
struct InstanceManager {
  using Descriptor = Descriptors<R(Args...)>;

  template <typename F>
  static constexpr Descriptor* get_instance() noexcept {
    using Impl = Impl<is_small<F>, F, R, Args...>;
    static Descriptor instance{
        .destroy = Impl::destroy,
        .copy = Impl::copy,
        .call = Impl::invoke,
        .move = Impl::move,
    };
    return &instance;
  }

  static constexpr Descriptor* get_empty_instance() noexcept {
    using Impl = EmptyImpl<R, Args...>;
    static Descriptor instance{
        .destroy = Impl::destroy,
        .copy = Impl::copy,
        .call = Impl::invoke,
        .move = Impl::move,
    };
    return &instance;
  }
};
} // namespace impl

template <typename F>
class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
  using Manager = impl::InstanceManager<R, Args...>;

public:
  Function() noexcept
      : buff{} {
    descriptors = Manager::get_empty_instance();
  }

  template <typename FF>
  Function(FF func) {
    using F = std::remove_cvref_t<FF>;
    descriptors = Manager::template get_instance<F>();
    if constexpr (impl::is_small<F>) {
      new (buff) F(std::forward<FF>(func));
    } else {
      new (buff) F*(new F(std::forward<FF>(func)));
    }
  }

  Function(const Function& other) {
    other.descriptors->copy(other.buff, buff);
    descriptors = other.descriptors;
  }

  Function(Function&& other) noexcept
      : Function() {
    descriptors = other.descriptors;
    descriptors->move(other.buff, buff);
    other.descriptors = Manager::get_empty_instance();
  }

  Function& operator=(const Function& other) {
    if (this != &other) {
      alignas(impl::SMALL_ALIGN) std::byte temp_buff[impl::SMALL_SIZE];
      std::memcpy(temp_buff, buff, impl::SMALL_SIZE);
      other.descriptors->copy(other.buff, buff);
      descriptors->destroy(temp_buff);
      descriptors = other.descriptors;
    }
    return *this;
  }

  Function& operator=(Function&& other) noexcept {
    if (this != &other) {
      alignas(impl::SMALL_ALIGN) std::byte temp_buff[impl::SMALL_SIZE];
      std::memcpy(temp_buff, buff, impl::SMALL_SIZE);
      other.descriptors->move(other.buff, buff);
      descriptors->destroy(temp_buff);
      descriptors = other.descriptors;
      other.descriptors = Manager::get_empty_instance();
    }
    return *this;
  }

  R operator()(Args... args) {
    return descriptors->call(buff, std::forward<Args>(args)...);
  }

  R operator()(Args&&... args) const {
    return descriptors->call(buff, std::forward<Args>(args)...);
  }

  explicit operator bool() const noexcept {
    return descriptors != Manager::get_empty_instance();
  }

  template <typename T>
  T* target() noexcept {
    return const_cast<T*>(std::as_const(*this).template target<T>());
  }

  template <typename T>
  const T* target() const noexcept {
    if constexpr (!std::is_invocable_r_v<R, T, Args...>) {
      return nullptr;
    } else {
      if (descriptors == Manager::template get_instance<T>()) {
        if constexpr (impl::is_small<T>) {
          return std::launder(reinterpret_cast<const T*>(buff));
        } else {
          return *std::launder(reinterpret_cast<const T**>(buff));
        }
      }
      return nullptr;
    }
  }

  ~Function() {
    descriptors->destroy(buff);
  }

private:
  alignas(impl::SMALL_ALIGN) mutable std::byte buff[impl::SMALL_SIZE];
  mutable impl::Descriptors<R(Args...)>* descriptors;
};

} // namespace ct
