#pragma once

#include "intrusive-list-element.h"
#include "intrusive-list-iterator.h"

#include <type_traits>

namespace ct::intrusive {
template <typename T, typename Tag = detail::DefaultTag>
class List {
  static_assert(std::is_base_of_v<ListElement<Tag>, T>, "T must derive from ListElement");

public:
  using Iterator = ListIterator<T, Tag>;
  using ConstIterator = ListIterator<const T, Tag>;

private:
  using Node = ListElement<Tag>;
  friend Iterator;
  friend ConstIterator;

public:
  // O(1)
  List() noexcept = default;

  // O(1)
  ~List() = default;

  List(const List&) = delete;

  List& operator=(const List&) = delete;

  // O(1)
  List(List&& other) noexcept = default;

  // O(1)
  List& operator=(List&& other) noexcept = default;

  // O(1)
  bool empty() const noexcept {
    return sentinel_.is_unlinked();
  }

  // O(n)
  size_t size() const noexcept {
    return std::distance(begin(), end());
  }

  // O(1)
  T& front() noexcept {
    return *begin();
  }

  // O(1)
  const T& front() const noexcept {
    return *begin();
  }

  // O(1)
  T& back() noexcept {
    return *(--end());
  }

  // O(1)
  const T& back() const noexcept {
    return *(--end());
  }

  // O(1)
  void push_front(T& value) noexcept {
    insert(begin(), value);
  }

  // O(1)
  void push_back(T& value) noexcept {
    insert(end(), value);
  }

  // O(1)
  void pop_front() noexcept {
    erase(begin());
  }

  // O(1)
  void pop_back() noexcept {
    erase(--end());
  }

  // O(1)
  void clear() noexcept {
    sentinel_.unlink();
  }

  // O(1)
  Iterator begin() noexcept {
    return Iterator(as_node(*sentinel_.next_));
  }

  // O(1)
  ConstIterator begin() const noexcept {
    return ConstIterator(as_node(*sentinel_.next_));
  }

  // O(1)
  Iterator end() noexcept {
    return Iterator(sentinel_);
  }

  // O(1)
  ConstIterator end() const noexcept {
    return ConstIterator(sentinel_);
  }

  // O(1)
  Iterator insert(ConstIterator pos, T& value) noexcept {
    Node& new_node = as_node(value);
    if (pos.node_ == &new_node) {
      return Iterator(new_node);
    }
    new_node.unlink();
    new_node.next_ = pos.node_;
    new_node.prev_ = pos.node_->prev_;
    pos.node_->prev_->next_ = &new_node;
    pos.node_->prev_ = &new_node;
    return Iterator(new_node);
  }

  // O(1)
  Iterator erase(ConstIterator pos) noexcept {
    Iterator next(as_node(*pos.node_->next_));
    pos.node_->unlink();
    return next;
  }

  // O(1)
  void splice(ConstIterator pos, List& /*other*/, ConstIterator first, ConstIterator last) noexcept {
    splice_impl(pos, first, last);
  }

private:
  void splice_impl(ConstIterator pos, ConstIterator first, ConstIterator last) noexcept {
    if (first == last) {
      return;
    }

    Node* before_last = &as_node(*last.node_->prev_);
    Node* before_first = &as_node(*first.node_->prev_);
    Node* before_pos = &as_node(*pos.node_->prev_);

    before_first->next_ = last.node_;
    last.node_->prev_ = before_first;

    first.node_->prev_ = before_pos;
    before_last->next_ = pos.node_;

    before_pos->next_ = first.node_;
    pos.node_->prev_ = before_last;
  }

  template <typename U>
  static decltype(auto) as_node(U& element) {
    return detail::as_node_impl<Tag>(element);
  }

  Node sentinel_;
};
} // namespace ct::intrusive
