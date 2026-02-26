#pragma once

#include "bimap-element.h"

#include <iterator>

namespace ct {
template <typename TLeft, typename TRight, typename CompareLeft, typename CompareRight>
class Bimap;
} // namespace ct

namespace ct::detail::iterator {
template <typename Side>
class IteratorImpl {
  using Base = element::ElementBase;
  using Key = typename Side::Key;
  using ElementTag = typename Side::ElementTag;
  using OppositeElementTag = typename Side::OppositeSide::ElementTag;
  using OppositeIterator = typename Side::OppositeSide::Iterator;
  using Element = typename Side::ElementType;
  using Sentinel = element::Sentinel;

public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using pointer = const Key*;
  using reference = const Key&;
  using iterator_tag = std::bidirectional_iterator_tag;

private:
  template <typename TLeft, typename TRight, typename CompareLeft, typename CompareRight>
  friend class ct::Bimap;

  friend OppositeIterator;

  IteratorImpl(const Base* node)
      : node_(const_cast<Base*>(node)) {}

public:
  IteratorImpl() = default;

  const Key& operator*() const {
    return static_cast<ElementTag*>(node_)->get_key();
  }

  const Key* operator->() const {
    return &operator*();
  }

  IteratorImpl& operator++() {
    // end()
    if (node_->is_sentinel()) {
      return *this;
    }
    if (Base::to_right(node_)) {
      while (Base::to_left(node_)) {}
      return *this;
    }
    if (node_->is_right_child()) {
      while (node_->is_right_child()) {
        Base::to_parent(node_);
      }
    }

    node_ = node_->parent_;

    return *this;
  }

  IteratorImpl operator++(int) {
    IteratorImpl tmp = *this;
    operator++();
    return tmp;
  }

  IteratorImpl& operator--() {
    // end()
    if (node_->is_sentinel()) {
      node_ = node_->left_;
      return *this;
    }

    if (Base::to_left(node_)) {
      while (Base::to_right(node_)) {}
      return *this;
    }
    if (node_->is_left_child()) {
      while (node_->is_left_child()) {
        Base::to_parent(node_);
      }
    }

    if (node_->parent_->is_sentinel()) {
      return *this;
    }
    node_ = node_->parent_;

    return *this;
  }

  IteratorImpl operator--(int) {
    IteratorImpl tmp = *this;
    operator--();
    return tmp;
  }

  friend bool operator==(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept {
    return lhs.node_ == rhs.node_;
  }

  friend bool operator!=(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept {
    return !(lhs.node_ == rhs.node_);
  }

  OppositeIterator flip() const {
    if (node_->is_sentinel()) {
      Sentinel* sentinel = Sentinel::to_sentinel<Side>(node_);
      return OppositeIterator(sentinel->to_base<typename Side::OppositeSide>());
    }
    return OppositeIterator(static_cast<OppositeElementTag*>(static_cast<Element*>(static_cast<ElementTag*>(node_))));
  }

private:
  Base* to_node() const noexcept {
    return node_;
  }

  Base* node_ = nullptr;
};
} // namespace ct::detail::iterator
