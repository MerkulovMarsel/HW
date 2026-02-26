#include "bimap-element.h"

namespace ct::detail::element {
ElementBase::ElementBase() = default;

ElementBase::ElementBase(ElementBase* parent, ElementBase* left, ElementBase* right)
    : parent_(parent)
    , left_(left)
    , right_(right) {}

ElementBase::ElementBase(ElementBase&& other) noexcept
    : parent_(other.parent_)
    , left_(other.left_)
    , right_(other.right_) {
  reset(other);
}

void ElementBase::reset(ElementBase& ptr) {
  ptr.parent_ = ptr.left_ = ptr.right_ = &ptr;
}

bool ElementBase::is_sentinel() const noexcept {
  if (!left_ || !right_) {
    return false;
  }
  // empty sentinel
  if (parent_ == this) {
    return true;
  }
  // root or sentinel
  if (parent_->parent_ == this) {
    // sentinel to tree, size() > 1 (not only root)
    if (left_->parent_ != this || right_->parent_ != this) {
      return true;
    }
    // only root map
    if (!parent_->left_ && !parent_->right_) {
      return true;
    }
  }
  return false;
}

bool ElementBase::to_left(ElementBase*& current) noexcept {
  if (!current || current->is_sentinel() || !current->has_left()) {
    return false;
  }
  current = current->left_;
  return true;
}

bool ElementBase::to_right(ElementBase*& current) noexcept {
  if (!current || current->is_sentinel() || !current->has_right()) {
    return false;
  }
  current = current->right_;
  return true;
}

bool ElementBase::to_parent(ElementBase*& current) noexcept {
  if (!current || current->is_sentinel() || !current->has_parent()) {
    return false;
  }
  current = current->parent_;
  return true;
}

bool ElementBase::has_parent() const noexcept {
  return parent_ && !parent_->is_sentinel();
}

bool ElementBase::has_left() const noexcept {
  return left_ && !left_->is_sentinel();
}

bool ElementBase::has_right() const noexcept {
  return right_ && !right_->is_sentinel();
}

void ElementBase::link_to_parent(
    ElementBase* parent,
    ElementBase* child,
    bool compare_result,
    bool overwrite
) noexcept {
  if (!parent) {
    return;
  }
  if (parent->is_sentinel()) {
    if (!child) {
      return;
    }
    parent->parent_ = child;
    parent->left_ = child;
    parent->right_ = child;
    child->parent_ = parent;
    return;
  }
  if (compare_result) {
    if (child && overwrite) {
      child->right_ = parent->right_;
      if (child->right_) {
        child->right_->parent_ = child;
      }
    }
    parent->right_ = child;
  } else {
    if (child && overwrite) {
      child->left_ = parent->left_;
      if (child->left_) {
        child->left_->parent_ = child;
      }
    }
    parent->left_ = child;
  }
  if (child) {
    child->parent_ = parent;
  }
}

void ElementBase::unlink(ElementBase* node) noexcept {
  if (!node || node->is_sentinel()) {
    return;
  }
  ElementBase* replace = nullptr;
  if (node->has_left()) {
    replace = node->left_;
    while (to_right(replace)) {}
    link_to_parent(replace, node->right_, true, false);
    if (replace->is_right_child()) {
      link_to_parent(replace->parent_, replace->left_, true, false);
      link_to_parent(replace, node->left_, false, false);
    }
  } else if (node->has_right()) {
    replace = node->right_;
  }
  if (node->is_right_child()) {
    node->parent_->right_ = replace;
  } else if (node->is_left_child()) {
    node->parent_->left_ = replace;
  } else {
    node->parent_->parent_ = (replace ? replace : node->parent_);
  }
  if (replace) {
    replace->parent_ = node->parent_;
  }
  reset(*node);
}

bool ElementBase::is_right_child() const noexcept {
  if (!has_parent()) {
    return false;
  }
  return parent_->right_ == this;
}

bool ElementBase::is_left_child() const noexcept {
  if (!has_parent()) {
    return false;
  }
  return parent_->left_ == this;
}

void swap(ElementBase& lhs, ElementBase& rhs) noexcept {
  using std::swap;
  swap(lhs.parent_, rhs.parent_);
  swap(lhs.left_, rhs.left_);
  swap(lhs.right_, rhs.right_);
}

} // namespace ct::detail::element
