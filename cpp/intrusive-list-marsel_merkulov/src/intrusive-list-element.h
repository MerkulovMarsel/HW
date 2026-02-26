#pragma once

namespace ct::intrusive {
namespace detail {
class DefaultTag;

struct ListElementImpl {
  ListElementImpl() = default;

  ListElementImpl(const ListElementImpl&) noexcept;

  ListElementImpl& operator=(const ListElementImpl&) noexcept;

  ListElementImpl(ListElementImpl&&) noexcept;

  ListElementImpl& operator=(ListElementImpl&&) noexcept;

  void unlink() noexcept;

  void replace(ListElementImpl* replacement) noexcept;

  bool is_unlinked() const noexcept;

  ListElementImpl* next_ = this;
  ListElementImpl* prev_ = this;
};
} // namespace detail

template <typename, typename>
class List;

template <typename, typename>
class ListIterator;

namespace detail {
template <typename, typename T>
constexpr auto& as_node_impl(T& obj) noexcept;

template <typename, typename T>
constexpr auto& as_node_impl(const T& obj) noexcept;

} // namespace detail

template <typename Tag = detail::DefaultTag>
class ListElement : detail::ListElementImpl {
  template <typename, typename>
  friend class List;

  template <typename, typename>
  friend class ListIterator;

  template <typename, typename T>
  friend constexpr auto& detail::as_node_impl(T& obj) noexcept;

  template <typename, typename T>
  friend constexpr auto& detail::as_node_impl(const T& obj) noexcept;
};

namespace detail {
template <typename Tag, typename T>
constexpr auto& as_node_impl(T& obj) noexcept {
  return static_cast<ListElement<Tag>&>(obj);
}

template <typename Tag, typename T>
constexpr auto& as_node_impl(const T& obj) noexcept {
  return static_cast<const ListElement<Tag>&>(obj);
}
} // namespace detail
} // namespace ct::intrusive
