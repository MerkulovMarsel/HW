#pragma once
#include "descriptor.h"

#include <any>
#include <cstddef>
#include <memory>
#include <new>

namespace ct {

template <typename T, detail::is_forward Tag>
class AnyIterator {
public:
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using difference_type = ptrdiff_t;
  using iterator_category = Tag;

private:
  using Storage = std::byte*;
  using ConstStorage = const std::byte*;
  using Manager = detail::InstanceManager<T, Tag>;

public:
  AnyIterator() = default;

  ~AnyIterator() {
    descriptor->destroy(buffer);
  }

  AnyIterator(const AnyIterator& other)
      : AnyIterator() {
    descriptor = other.descriptor;
    descriptor->copy(other.buffer, buffer);
  }

  AnyIterator(AnyIterator&& other) noexcept
      : AnyIterator() {
    descriptor = other.descriptor;
    descriptor->move(other.buffer, buffer);
    other.descriptor = Manager::get_empty_instance();
  }

  AnyIterator& operator=(AnyIterator&& other) noexcept {
    if (descriptor == other.descriptor && descriptor) {
      descriptor->move_as(other.buffer, buffer);
      if (this != &other) {
        other.descriptor = Manager::get_empty_instance();
      }
      return *this;
    }
    AnyIterator temporary(std::move(other));
    swap(temporary);
    return *this;
  }

  AnyIterator& operator=(const AnyIterator& other) {
    if (descriptor == other.descriptor && descriptor) {
      descriptor->copy_as(other.buffer, buffer);
      return *this;
    }
    AnyIterator temporary(other);
    swap(temporary);
    return *this;
  }

  template <typename It>
  AnyIterator(It it)
      : descriptor(Manager::template get_instance<It>()) {
    if constexpr (detail::is_small<It>) {
      new (buffer) It(std::forward<It>(it));
    } else {
      new (buffer) It*(new It(std::forward<It>(it)));
    }
  }

  template <typename It>
  AnyIterator& operator=(It it) {
    AnyIterator temp(it);
    swap(temp);
    return *this;
  }

  void swap(AnyIterator& other) noexcept {
    if (descriptor == other.descriptor && descriptor) {
      descriptor->swap(other.buffer, buffer);
      return;
    }
    std::swap(descriptor, other.descriptor);
    std::swap(buffer, other.buffer);
  }

  T& operator*() const {
    return descriptor->get_ref(const_cast<Storage>(buffer));
  }

  const T* operator->() const {
    return descriptor->get(const_cast<Storage>(buffer));
  }

  T* operator->() {
    return descriptor->get(buffer);
  }

  AnyIterator& operator++() & {
    descriptor->inc(buffer);
    return *this;
  }

  AnyIterator operator++(int) & {
    AnyIterator temp = *this;
    ++*this;
    return temp;
  }

  reference operator[](difference_type diff) const
    requires detail::is_random<Tag>
  {
    return descriptor->accs(const_cast<Storage>(buffer), diff);
  }

  template <typename TT, typename TTag>
  friend bool operator==(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b);

  template <typename TT, typename TTag>
  friend bool operator!=(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b);

  // note: following operators must compile ONLY for appropriate iterator tags

  template <typename TT, detail::is_bidirectional TTag>
  friend AnyIterator<TT, TTag>& operator--(AnyIterator<TT, TTag>& it);

  template <typename TT, detail::is_bidirectional TTag>
  friend AnyIterator<TT, TTag> operator--(AnyIterator<TT, TTag>& it, int);

  template <typename TT, detail::is_random TTag>
  friend AnyIterator<TT, TTag>&
  operator+=(AnyIterator<TT, TTag>& it, typename AnyIterator<TT, TTag>::difference_type diff);

  template <typename TT, detail::is_random TTag>
  friend AnyIterator<TT, TTag>&
  operator-=(AnyIterator<TT, TTag>& it, typename AnyIterator<TT, TTag>::difference_type diff);

  template <typename TT, detail::is_random TTag>
  friend AnyIterator<TT, TTag>
  operator+(AnyIterator<TT, TTag> it, typename AnyIterator<TT, TTag>::difference_type diff);

  template <typename TT, detail::is_random TTag>
  friend AnyIterator<TT, TTag>
  operator-(AnyIterator<TT, TTag> it, typename AnyIterator<TT, TTag>::difference_type diff);

  template <typename TT, detail::is_random TTag>
  friend bool operator<(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b);

  template <typename TT, detail::is_random TTag>
  friend bool operator>(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b);

  template <typename TT, detail::is_random TTag>
  friend typename AnyIterator<TT, TTag>::difference_type
  operator-(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b);

private:
  alignas(detail::SMALL_ALIGN) std::byte buffer[detail::SMALL_SIZE];
  detail::Descriptors<T, Tag>* descriptor = Manager::get_empty_instance();
};

template <typename TT, typename TTag>
bool operator==(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  if (a.descriptor != b.descriptor) {
    return false;
  }
  return a.descriptor->eq(a.buffer, b.buffer);
}

template <typename TT, typename TTag>
bool operator!=(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  if (a.descriptor != b.descriptor) {
    return true;
  }
  return a.descriptor->not_equ(a.buffer, b.buffer);
}

template <typename TT, detail::is_bidirectional TTag>
AnyIterator<TT, TTag>& operator--(AnyIterator<TT, TTag>& it) {
  it.descriptor->decr(it.buffer);
  return it;
}

template <typename TT, detail::is_bidirectional TTag>
AnyIterator<TT, TTag> operator--(AnyIterator<TT, TTag>& it, int) {
  AnyIterator<TT, TTag> temp = it;
  --it;
  return temp;
}

template <typename TT, detail::is_random TTag>
AnyIterator<TT, TTag>& operator+=(AnyIterator<TT, TTag>& it, typename AnyIterator<TT, TTag>::difference_type diff) {
  it.descriptor->add(it.buffer, diff);
  return it;
}

template <typename TT, detail::is_random TTag>
AnyIterator<TT, TTag>& operator-=(AnyIterator<TT, TTag>& it, typename AnyIterator<TT, TTag>::difference_type diff) {
  it.descriptor->sub(it.buffer, diff);
  return it;
}

template <typename TT, detail::is_random TTag>
AnyIterator<TT, TTag> operator+(AnyIterator<TT, TTag> it, typename AnyIterator<TT, TTag>::difference_type diff) {
  return AnyIterator<TT, TTag>(it += diff);
}

template <typename TT, detail::is_random TTag>
AnyIterator<TT, TTag> operator-(AnyIterator<TT, TTag> it, typename AnyIterator<TT, TTag>::difference_type diff) {
  return AnyIterator<TT, TTag>(it -= diff);
}

template <typename TT, detail::is_random TTag>
AnyIterator<TT, TTag> operator+(typename AnyIterator<TT, TTag>::difference_type diff, AnyIterator<TT, TTag> it) {
  return it + diff;
}

template <typename TT, detail::is_random TTag>
AnyIterator<TT, TTag> operator-(typename AnyIterator<TT, TTag>::difference_type diff, AnyIterator<TT, TTag> it) {
  return it - diff;
}

template <typename TT, detail::is_random TTag>
bool operator<(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  return a.descriptor->less(a.buffer, b.buffer);
}

template <typename TT, detail::is_random TTag>
bool operator>(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  return a.descriptor->greater(a.buffer, b.buffer);
}

template <typename TT, detail::is_random TTag>
bool operator>=(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  return !(a < b);
}

template <typename TT, detail::is_random TTag>
bool operator<=(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  return (a < b) || (a == b);
}

template <typename TT, detail::is_random TTag>
typename AnyIterator<TT, TTag>::difference_type
operator-(const AnyIterator<TT, TTag>& a, const AnyIterator<TT, TTag>& b) {
  return a.descriptor->diff(a.buffer, b.buffer);
}

} // namespace ct
