#pragma once

#include "bimap-element.h"
#include "bimap-iterator.h"

#include <stdexcept>

namespace ct {
template <
    typename TLeft,
    typename TRight,
    typename CompareLeft = std::less<TLeft>,
    typename CompareRight = std::less<TRight>>
class Bimap {
  using Left = TLeft;
  using Right = TRight;

  using Base = detail::element::ElementBase;
  using Sentinel = detail::element::Sentinel;

  template <typename T>
  using IteratorImpl = detail::iterator::IteratorImpl<T>;

  class Element;

  struct LeftTag {};

  struct RightTag {};

  template <typename Tag>
    requires std::is_same_v<Tag, LeftTag> || std::is_same_v<Tag, RightTag>
  struct SideTrait {
    static constexpr bool is_left = std::is_same_v<Tag, LeftTag>;
    using OppositeTag = std::conditional_t<is_left, RightTag, LeftTag>;
    using Key = std::conditional_t<is_left, Left, Right>;
    using Value = std::conditional_t<is_left, Right, Left>;
    using Compare = std::conditional_t<is_left, CompareLeft, CompareRight>;
    using ElementTag = detail::element::ElementTag<SideTrait>;
    using ElementType = Element;
    using Iterator = IteratorImpl<SideTrait>;
    using OppositeSide = SideTrait<OppositeTag>;
  };

  using LeftSide = SideTrait<LeftTag>;
  using RightSide = SideTrait<RightTag>;

  using LeftElement = typename LeftSide::ElementTag;
  using RightElement = typename RightSide::ElementTag;

  class Element
      : public LeftElement
      , public RightElement {
  public:
    template <typename LeftValue, typename RightValue>
    Element(LeftValue&& left, RightValue&& right)
        : LeftElement(std::forward<LeftValue>(left))
        , RightElement(std::forward<RightValue>(right)) {}
  };

public:
  using LeftIterator = IteratorImpl<LeftSide>;
  using RightIterator = IteratorImpl<RightSide>;

public:
  explicit Bimap(CompareLeft compare_left = CompareLeft(), CompareRight compare_right = CompareRight())
      : compare_left_(std::move(compare_left))
      , compare_right_(std::move(compare_right)) {}

  Bimap(const Bimap& other)
      : Bimap(other.compare_left_, other.compare_right_) {
    for (auto it = other.begin_left(); it != other.end_left(); ++it) {
      insert(*it, *(it.flip()));
    }
  }

  Bimap(Bimap&& other) noexcept
      : Bimap(std::move(other.compare_left_), std::move(other.compare_right_)) {
    swap_without_compare(*this, other);
  }

  Bimap& operator=(const Bimap& other) {
    if (this != &other) {
      Bimap tmp(other);
      clear();
      swap(*this, tmp);
    }
    return *this;
  }

  Bimap& operator=(Bimap&& other) noexcept {
    if (this != &other) {
      Bimap tmp(std::move(other));
      clear();
      swap(*this, tmp);
    }
    return *this;
  }

  ~Bimap() {
    clear();
  }

  friend void swap(Bimap& lhs, Bimap& rhs) noexcept {
    using std::swap;
    swap(lhs.compare_left_, rhs.compare_left_);
    swap(lhs.compare_right_, rhs.compare_right_);
    swap_without_compare(lhs, rhs);
  }

  LeftIterator insert(const Left& left, const Right& right) {
    return insert_impl(left, right);
  }

  LeftIterator insert(const Left& left, Right&& right) {
    return insert_impl(left, std::move(right));
  }

  LeftIterator insert(Left&& left, const Right& right) {
    return insert_impl(std::move(left), right);
  }

  LeftIterator insert(Left&& left, Right&& right) {
    return insert_impl(std::move(left), std::move(right));
  }

  LeftIterator erase_left(LeftIterator it) {
    return left().erase_it_impl(it);
  }

  RightIterator erase_right(RightIterator it) {
    return right().erase_it_impl(it);
  }

  bool erase_left(const Left& left_value) {
    return left().erase_val_impl(left_value);
  }

  bool erase_right(const Right& right_value) {
    return right().erase_val_impl(right_value);
  }

  LeftIterator erase_left(LeftIterator first, LeftIterator last) {
    return left().erase_it_impl(first, last);
  }

  RightIterator erase_right(RightIterator first, RightIterator last) {
    return right().erase_it_impl(first, last);
  }

  LeftIterator find_left(const Left& left_value) const {
    return left().find_impl(left_value);
  }

  RightIterator find_right(const Right& right_value) const {
    return right().find_impl(right_value);
  }

  const Right& at_left(const Left& key) const {
    return left().at_impl(key);
  }

  const Left& at_right(const Right& key) const {
    return right().at_impl(key);
  }

  const Right& at_left_or_default(const Left& key) {
    return left().at_or_default(key);
  }

  const Left& at_right_or_default(const Right& key) {
    return right().at_or_default(key);
  }

  LeftIterator lower_bound_left(const Left& left_value) const {
    return left().template bound_impl<true>(left_value);
  }

  LeftIterator upper_bound_left(const Left& left_value) const {
    return left().template bound_impl<false>(left_value);
  }

  RightIterator lower_bound_right(const Right& right_value) const {
    return right().template bound_impl<true>(right_value);
  }

  RightIterator upper_bound_right(const Right& right_value) const {
    return right().template bound_impl<false>(right_value);
  }

  LeftIterator begin_left() const {
    return LeftIterator(sentinel_.to_base<LeftSide>()->right_);
  }

  LeftIterator end_left() const {
    return LeftIterator(sentinel_.to_base<LeftSide>());
  }

  RightIterator begin_right() const {
    return RightIterator(sentinel_.to_base<RightSide>()->right_);
  }

  RightIterator end_right() const {
    return RightIterator(sentinel_.to_base<RightSide>());
  }

  bool empty() const noexcept {
    return size() == 0;
  }

  std::size_t size() const noexcept {
    return size_;
  }

  friend bool operator==(const Bimap& lhs, const Bimap& rhs) {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (auto l_it = lhs.begin_left(), r_it = rhs.begin_left(); l_it != lhs.end_left(); ++l_it, ++r_it) {
      if (!lhs.left().is_equal(*l_it, *r_it) || !lhs.right().is_equal(*l_it.flip(), *r_it.flip())) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(const Bimap& lhs, const Bimap& rhs) {
    return !(lhs == rhs);
  }

private:
  friend void swap_without_compare(Bimap& lhs, Bimap& rhs) noexcept {
    using std::swap;
    swap(lhs.size_, rhs.size_);
    swap(lhs.sentinel_, rhs.sentinel_);
    lhs.update_parents();
    rhs.update_parents();
  }

  void update_parents() noexcept {
    if (empty()) {
      return;
    }
    sentinel_.to_base<LeftSide>()->parent_->parent_ = sentinel_.to_base<LeftSide>();
    sentinel_.to_base<RightSide>()->parent_->parent_ = sentinel_.to_base<RightSide>();
  }

  void clear() noexcept {
    erase_left(begin_left(), end_left());
  }

  template <typename ValueLeft, typename ValueRight>
  LeftIterator insert_impl(ValueLeft&& left_value, ValueRight&& right_value, bool tolerant = false) {
    auto left_pos = left().find_insert_position(left_value);
    auto right_pos = right().find_insert_position(right_value);

    if (!tolerant && !empty()) {
      if (left().is_equal(*left_pos, std::forward<ValueLeft>(left_value)) ||
          right().is_equal(*right_pos, std::forward<ValueRight>(right_value))) {
        return end_left();
      }
    }
    return insert_at(std::forward<ValueLeft>(left_value), std::forward<ValueRight>(right_value), left_pos, right_pos);
  }

  template <typename ValueLeft, typename ValueRight>
  LeftIterator insert_at(
      ValueLeft&& left_value,
      ValueRight&& right_value,
      const LeftIterator& pos_left,
      const RightIterator& pos_right
  ) {
    bool compare_left = false;
    bool compare_right = false;
    if (!empty()) {
      compare_left = compare_left_(*pos_left, left_value);
      compare_right = compare_right_(*pos_right, right_value);
    }

    auto* element = new Element(std::forward<ValueLeft>(left_value), std::forward<ValueRight>(right_value));

    LeftElement::link_to_parent(pos_left.to_node(), static_cast<LeftElement*>(element), compare_left);

    RightElement::link_to_parent(pos_right.to_node(), static_cast<RightElement*>(element), compare_right);

    left().update_sentinel();
    right().update_sentinel();
    ++size_;
    return LeftIterator(static_cast<LeftElement*>(element));
  }

private:
  size_t size_ = 0;
  Sentinel sentinel_;
  [[no_unique_address]] CompareLeft compare_left_;
  [[no_unique_address]] CompareRight compare_right_;

private:
  template <typename Side>
  auto get_view() const noexcept {
    if constexpr (Side::is_left) {
      return left();
    } else {
      return right();
    }
  }

  auto left() const noexcept {
    return View<LeftSide>(*this);
  }

  auto right() const noexcept {
    return View<RightSide>(*this);
  }

  template <typename Side>
  class View {
    using Iterator = typename Side::Iterator;
    using Key = typename Side::Key;
    using Value = typename Side::Value;
    using Compare = typename Side::Compare;
    using OppositeSide = typename Side::OppositeSide;
    using ElementTag = typename Side::ElementTag;
    using OppositeElementTag = typename Side::OppositeSide::ElementTag;

    Bimap& map;

  private:
    static ElementTag* to_element_tag(Base* base) {
      return static_cast<ElementTag*>(base);
    }

  public:
    explicit View(const Bimap& map)
        : map(const_cast<Bimap&>(map)) {}

    Base& get_sentinel() const noexcept {
      return *(map.sentinel_.to_base<Side>());
    }

    Base* get_root() const noexcept {
      return (map.sentinel_.to_base<Side>())->parent_;
    }

    Iterator get_end() const noexcept {
      return Iterator(get_root()->parent_);
    }

    Iterator get_begin() const noexcept {
      return Iterator(get_root()->parent_->right_);
    }

    const Compare& get_compare() const noexcept {
      if constexpr (Side::is_left) {
        return map.compare_left_;
      } else {
        return map.compare_right_;
      }
    }

    void update_sentinel(Base* old = nullptr) {
      auto& sentinel = get_sentinel();
      if (old) {
        if (sentinel.parent_ == old && !old->left_ && !old->right_) {
          // erase root, root is last element
          sentinel.left_ = sentinel.right_ = &sentinel;
          return;
        }
        Iterator old_it = old;
        if (old == sentinel.left_) {
          // erase max
          --old_it;
          sentinel.left_ = old_it.to_node();
          return;
        }
        if (old == sentinel.right_) {
          // erase min
          ++old_it;
          sentinel.right_ = old_it.to_node();
          return;
        }
        return;
      }
      if (map.empty()) {
        // insert first element
        sentinel.left_ = sentinel.parent_;
        sentinel.right_ = sentinel.parent_;
        return;
      }
      if (sentinel.left_->has_right()) {
        // insert new max
        sentinel.left_ = sentinel.left_->right_;
        return;
      }
      if (sentinel.right_->has_left()) {
        // insert new min
        sentinel.right_ = sentinel.right_->left_;
      }
    }

    bool is_equal(const Key& lhs, const Key& rhs) const {
      return compare(lhs, rhs) == 0;
    }

    bool is_greater(const Key& lhs, const Key& rhs) const {
      return compare(lhs, rhs) < 0;
    }

    bool is_less(const Key& lhs, const Key& rhs) const {
      return compare(lhs, rhs) > 0;
    }

    int compare(const Key& lhs, const Key& rhs) const {
      auto& comparator = get_compare();
      if (comparator(lhs, rhs)) {
        return 1;
      }
      if (comparator(rhs, lhs)) {
        return -1;
      }
      return 0;
    }

    template <bool is_lower_bound>
    Iterator bound_impl(const Key& key) const {
      auto position = find_insert_position(key);
      if (position == get_end()) {
        return position;
      }
      if constexpr (is_lower_bound) {
        return compare(*position, key) <= 0 ? position : ++position;
      } else {
        return is_greater(*position, key) ? position : ++position;
      }
    }

    const Value& at_or_default(const Key& key)
      requires (!std::is_default_constructible_v<Value>)
    {
      auto position = find_insert_position(key);
      if (is_equal(*position, key)) {
        return *(position.flip());
      }
      throw std::out_of_range("key not found and no default constructor");
    }

    const Value& at_or_default(const Key& key)
      requires std::is_default_constructible_v<Value>
    {
      auto position = find_insert_position(key);
      if (is_equal(*position, key)) {
        return *(position.flip());
      }
      Value value = {};
      auto position_value = map.get_view<OppositeSide>().find_insert_position(value);

      if (!map.get_view<OppositeSide>().is_equal(*position_value, value)) {
        if constexpr (Side::is_left) {
          return *map.insert_at(key, std::move(value), position, position_value).flip();
        } else {
          return *map.insert_at(std::move(value), key, position_value, position);
        }
      }

      LeftIterator new_element;
      if constexpr (Side::is_left) {
        new_element = map.insert_impl(key, std::move(value), true);
      } else {
        new_element = map.insert_impl(std::move(value), key, true);
      }

      map.get_view<OppositeSide>().erase_it_impl(position_value);

      if constexpr (Side::is_left) {
        return *new_element.flip();
      } else {
        return *new_element;
      }
    }

    Iterator erase_it_impl(Iterator it_start, Iterator it_end) noexcept {
      while (it_start != it_end && !map.empty()) {
        auto* current = static_cast<Element*>(to_element_tag(it_start.to_node()));

        ++it_start;
        map.left().update_sentinel(static_cast<Base*>(static_cast<LeftElement*>(current)));
        map.right().update_sentinel(static_cast<Base*>(static_cast<RightElement*>(current)));
        ElementTag::unlink(static_cast<ElementTag*>(current));
        OppositeElementTag::unlink(static_cast<OppositeElementTag*>(current));
        delete current;
        --map.size_;
      }

      return it_end;
    }

    Iterator erase_it_impl(Iterator it_pos) noexcept {
      auto next = it_pos;
      ++next;
      return erase_it_impl(it_pos, next);
    }

    bool erase_val_impl(const Key& key) {
      auto erasable = find_insert_position(key);
      if (!is_equal(*erasable, key)) {
        return false;
      }
      erase_it_impl(erasable);
      return true;
    }

    Iterator find_impl(const Key& key) const {
      auto position = find_insert_position(key);
      if (!map.empty() && is_equal(*position, key)) {
        return position;
      }
      return get_end();
    }

    const Value& at_impl(const Key& key) const {
      auto position = find_impl(key);
      if (position == get_end()) {
        throw std::out_of_range("key not found");
      }
      return *(position.flip());
    }

    Iterator find_insert_position(const Key& value) const {
      Base* root = get_root();
      if (map.empty()) {
        return get_end();
      }

      auto* current = to_element_tag(root);
      while (true) {
        if (is_less(current->get_key(), value)) {
          if (!current->has_right()) {
            return current;
          }
          current = to_element_tag(current->right_);
          continue;
        }
        if (is_greater(current->get_key(), value)) {
          if (!current->has_left()) {
            return current;
          }
          current = to_element_tag(current->left_);
          continue;
        }
        return current;
      }
      std::unreachable();
    }
  };
};
} // namespace ct
