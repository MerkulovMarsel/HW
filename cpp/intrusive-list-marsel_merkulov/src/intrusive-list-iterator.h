#pragma once

#include "intrusive-list-element.h"

#include <iterator>

namespace ct::intrusive {
template <typename T, typename Tag>
class List;

template <typename T, typename Tag>
class ListIterator {
  static_assert(std::is_base_of_v<ListElement<Tag>, T>, "T must derive from ListElement");

public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using reference = T&;

private:
  using Node = ListElement<Tag>;

  friend class ListIterator<const T, Tag>;
  friend class ListIterator<std::remove_cv_t<T>, Tag>;

  friend class List<const T, Tag>;
  friend class List<std::remove_cv_t<T>, Tag>;

public:
  ListIterator() noexcept = default;

  ListIterator(const ListIterator&) noexcept = default;

  ListIterator& operator=(const ListIterator&) noexcept = default;

  operator ListIterator<const T, Tag>() const noexcept {
    return ListIterator<const T, Tag>(*node_);
  }

  reference operator*() const {
    return static_cast<reference>(*node_);
  }

  pointer operator->() const {
    return static_cast<pointer>(node_);
  }

  ListIterator& operator++() noexcept {
    if (node_) {
      node_ = &as_node(*node_->next_);
    }
    return *this;
  }

  ListIterator operator++(int) noexcept {
    ListIterator result = *this;
    operator++();
    return result;
  }

  ListIterator& operator--() noexcept {
    if (node_) {
      node_ = &as_node(*node_->prev_);
    }
    return *this;
  }

  ListIterator operator--(int) noexcept {
    ListIterator result = *this;
    operator--();
    return result;
  }

  friend bool operator==(const ListIterator& left, const ListIterator& right) noexcept {
    return left.node_ == right.node_;
  }

  friend bool operator!=(const ListIterator& left, const ListIterator& right) noexcept {
    return !(left == right);
  }

  void swap(ListIterator& other) noexcept {
    std::swap(node_, other.node_);
  }

private:
  template <typename U>
  static decltype(auto) as_node(U& element) {
    return detail::as_node_impl<Tag>(element);
  }

  explicit ListIterator(Node& node) noexcept
      : node_(&node) {}

  explicit ListIterator(const Node& node) noexcept
      : node_(const_cast<Node*>(&node)) {}

  Node* node_;
};
} // namespace ct::intrusive
