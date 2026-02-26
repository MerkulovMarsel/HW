#pragma once

#include <any>
#include <cstddef>
#include <memory>
#include <new>

namespace detail {

class BadAnyIteratorGet : public std::exception {
  const char* what() const noexcept override {
    return "get empty AnyIterator";
  }
};

static constexpr size_t SMALL_SIZE = sizeof(void*);
static constexpr size_t SMALL_ALIGN = alignof(void*);

template <typename It>
concept is_small = (sizeof(It) <= SMALL_SIZE) && (alignof(It) <= SMALL_ALIGN);

template <typename Tag>
concept is_random = std::is_same_v<Tag, std::random_access_iterator_tag>;
template <typename Tag>
concept is_bidirectional = is_random<Tag> || std::is_same_v<Tag, std::bidirectional_iterator_tag>;
template <typename Tag>
concept is_forward = is_bidirectional<Tag> || std::is_base_of_v<Tag, std::forward_iterator_tag>;

template <typename It, typename T, typename Tag>
struct SmallBase {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;
  using pointer = T*;
  using reference = T&;
  using difference_type = ptrdiff_t;

  static void destroy(Storage storage) {
    get_it(storage)->~It();
  }

  static void copy(ConstStorage src, Storage dst) {
    new (dst) It(*get_it(src));
  }

  static void move(Storage src, Storage dst) noexcept {
    It* src_obj = get_it(src);
    new (dst) It(std::move(*src_obj));
    src_obj->~It();
  }

  static It* get_it(Storage& storage) {
    return std::launder(reinterpret_cast<It*>(storage));
  }

  static const It* get_it(ConstStorage& storage) {
    return std::launder(reinterpret_cast<const It*>(storage));
  }
};

template <typename It, typename T, typename Tag>
struct BigBase {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;
  using pointer = T*;
  using reference = T&;
  using difference_type = ptrdiff_t;

  static void destroy(Storage storage) {
    delete get_it(storage);
  }

  static void copy(ConstStorage src, Storage dst) {
    new (dst) It*(new It(*get_it(src)));
  }

  static void move(Storage src, Storage dst) noexcept {
    It* ptr = get_it(src);
    new (dst) It*(ptr);
  }

  static It* get_it(Storage& storage) {
    return *std::launder(reinterpret_cast<It**>(storage));
  }

  static const It* get_it(ConstStorage& storage) {
    return *std::launder(reinterpret_cast<It* const*>(storage));
  }
};

template <typename Base, typename It, typename T, typename Tag>
struct ImplCommon {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;
  using pointer = T*;
  using reference = T&;

  static void destroy(Storage storage) {
    Base::destroy(storage);
  }

  static void copy(ConstStorage src, Storage dst) {
    Base::copy(src, dst);
  }

  static void move(Storage src, Storage dst) noexcept {
    Base::move(src, dst);
  }

  static void move_as(Storage dst, Storage src) noexcept {
    Base::get_it(src)->operator=(std::move(*Base::get_it(dst)));
  }

  static void copy_as(ConstStorage dst, Storage src) {
    Base::get_it(src)->operator=(*Base::get_it(dst));
  }

  static void swap(Storage src, Storage dst) noexcept {
    It* src_obj = Base::get_it(src);
    It* dst_obj = Base::get_it(dst);
    std::swap(*src_obj, *dst_obj);
  }

  static bool eq(ConstStorage lhs, ConstStorage rhs) noexcept {
    const It* lhs_obj = Base::get_it(lhs);
    const It* rhs_obj = Base::get_it(rhs);
    return *lhs_obj == *rhs_obj;
  }

  static bool not_equ(ConstStorage lhs, ConstStorage rhs) noexcept {
    const It* lhs_obj = Base::get_it(lhs);
    const It* rhs_obj = Base::get_it(rhs);
    return *lhs_obj != *rhs_obj;
  }

  static pointer get(Storage src) noexcept {
    return Base::get_it(src)->operator->();
  }

  static reference get_ref(Storage src) noexcept {
    return **Base::get_it(src);
  }

  static void inc(Storage src) {
    It& src_obj = *Base::get_it(src);
    ++src_obj;
  }

  static void decr(Storage src)
    requires is_bidirectional<Tag>
  {
    It& src_obj = *Base::get_it(src);
    --src_obj;
  }

  static void add(Storage src, ptrdiff_t diff)
    requires is_random<Tag>
  {
    It& src_obj = *Base::get_it(src);
    src_obj += diff;
  }

  static void sub(Storage src, ptrdiff_t diff)
    requires is_random<Tag>
  {
    It& src_obj = *Base::get_it(src);
    src_obj -= diff;
  }

  static bool less(ConstStorage lhs, ConstStorage rhs) noexcept
    requires is_random<Tag>
  {
    return *Base::get_it(lhs) < *Base::get_it(rhs);
  }

  static bool greater(ConstStorage lhs, ConstStorage rhs) noexcept
    requires is_random<Tag>
  {
    return *Base::get_it(lhs) > *Base::get_it(rhs);
  }

  static ptrdiff_t diff(ConstStorage lhs, ConstStorage rhs) noexcept
    requires is_random<Tag>
  {
    return *Base::get_it(lhs) - *Base::get_it(rhs);
  }

  static reference accs(Storage src, std::ptrdiff_t diff)
    requires is_random<Tag>
  {
    return Base::get_it(src)->operator[](diff);
  }
};

template <typename T, typename Tag>
struct EmptyImpl {
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;
  using tag = Tag;
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using difference_type = ptrdiff_t;

  static void destroy(Storage) {}

  static void copy(ConstStorage, Storage) {}

  static void move(Storage, Storage) noexcept {}

  static void move_as(Storage, Storage) noexcept {}

  static void copy_as(ConstStorage, Storage) {}

  static void swap(Storage, Storage) noexcept {}

  static bool eq(ConstStorage, ConstStorage) noexcept {
    return false;
  }

  static bool not_equ(ConstStorage, ConstStorage) noexcept {
    return false;
  }

  static pointer get(Storage) {
    throw BadAnyIteratorGet{};
  }

  static reference get_ref(Storage) {
    throw BadAnyIteratorGet{};
  }

  static void inc(Storage) {
    throw BadAnyIteratorGet{};
  }

  static void decr(Storage)
    requires is_bidirectional<Tag>
  {}

  static void add(Storage, ptrdiff_t)
    requires is_random<Tag>
  {}

  static void sub(Storage, ptrdiff_t)
    requires is_random<Tag>
  {}

  static bool less(ConstStorage, ConstStorage) noexcept
    requires is_random<Tag>
  {
    return false;
  }

  static bool greater(ConstStorage, ConstStorage) noexcept
    requires is_random<Tag>
  {
    return false;
  }

  static ptrdiff_t diff(ConstStorage, ConstStorage) noexcept
    requires is_random<Tag>
  {
    return ptrdiff_t{};
  }

  static reference accs(Storage, std::ptrdiff_t)
    requires is_random<Tag>
  {
    throw BadAnyIteratorGet{};
  }
};

template <typename It, typename T, typename Tag>
using impl_t = ImplCommon<std::conditional_t<is_small<It>, SmallBase<It, T, Tag>, BigBase<It, T, Tag>>, It, T, Tag>;

template <typename T, typename Tag>
struct Descriptors;

template <typename T>
struct Descriptors<T, std::forward_iterator_tag> {
  using Destroy = void (*)(std::byte*);
  using Copy = void (*)(const std::byte*, std::byte*);
  using CopyAssign = void (*)(const std::byte*, std::byte*);
  using Move = void (*)(std::byte*, std::byte*) noexcept;
  using MoveAssign = void (*)(std::byte*, std::byte*) noexcept;
  using Swap = void (*)(std::byte*, std::byte*) noexcept;
  using Equal = bool (*)(const std::byte*, const std::byte*) noexcept;
  using Not_Equal = bool (*)(const std::byte*, const std::byte*) noexcept;
  using Get = T* (*) (std::byte*);
  using GetRef = T& (*) (std::byte*);
  using Inc = void (*)(std::byte*);

  Destroy destroy = nullptr;
  Copy copy = nullptr;
  CopyAssign copy_as = nullptr;
  Move move = nullptr;
  MoveAssign move_as = nullptr;
  Swap swap = nullptr;
  Equal eq = nullptr;
  Not_Equal not_equ = nullptr;
  Get get = nullptr;
  GetRef get_ref = nullptr;
  Inc inc = nullptr;
};

template <typename T>
struct Descriptors<T, std::bidirectional_iterator_tag> : Descriptors<T, std::forward_iterator_tag> {
  using Decr = void (*)(std::byte*);
  Decr decr = nullptr;
};

template <typename T>
struct Descriptors<T, std::random_access_iterator_tag> : Descriptors<T, std::bidirectional_iterator_tag> {
  using Add = void (*)(std::byte*, ptrdiff_t);
  using Sub = void (*)(std::byte*, ptrdiff_t);
  using Less = bool (*)(const std::byte*, const std::byte*);
  using Greater = bool (*)(const std::byte*, const std::byte*);
  using Diff = ptrdiff_t (*)(const std::byte*, const std::byte*);
  using Access = T& (*) (std::byte*, std::ptrdiff_t);

  Add add = nullptr;
  Sub sub = nullptr;
  Less less = nullptr;
  Greater greater = nullptr;
  Diff diff = nullptr;
  Access accs = nullptr;
};

template <typename T, typename Tag>
struct InstanceManager {
  using Descriptor = Descriptors<T, Tag>;

  template <typename It, typename Impl = impl_t<It, T, Tag>>
  static constexpr Descriptor* get_instance() noexcept {
    static Descriptor instance;
    instance.destroy = Impl::destroy;
    instance.copy = Impl::copy;
    instance.copy_as = Impl::copy_as;
    instance.move = Impl::move;
    instance.move_as = Impl::move_as;
    instance.swap = Impl::swap;
    instance.eq = Impl::eq;
    instance.not_equ = Impl::not_equ;
    instance.get = Impl::get;
    instance.get_ref = Impl::get_ref;
    instance.inc = Impl::inc;
    if constexpr (is_bidirectional<Tag>) {
      instance.decr = Impl::decr;
    }
    if constexpr (is_random<Tag>) {
      instance.add = Impl::add;
      instance.sub = Impl::sub;
      instance.less = Impl::less;
      instance.greater = Impl::greater;
      instance.diff = Impl::diff;
      instance.accs = Impl::accs;
    }
    return &instance;
  }

  static constexpr Descriptor* get_empty_instance() noexcept {
    return get_instance<void, EmptyImpl<T, Tag>>();
  }
};

} // namespace detail
