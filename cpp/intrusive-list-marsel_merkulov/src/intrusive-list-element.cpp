#include "intrusive-list-element.h"

namespace ct::intrusive::detail {
ListElementImpl::ListElementImpl(const ListElementImpl&) noexcept
    : ListElementImpl() {}

ListElementImpl& ListElementImpl::operator=(const ListElementImpl& other) noexcept {
  if (this != &other) {
    unlink();
  }
  return *this;
}

ListElementImpl::ListElementImpl(ListElementImpl&& other) noexcept {
  other.replace(this);
}

ListElementImpl& ListElementImpl::operator=(ListElementImpl&& other) noexcept {
  if (this != &other) {
    other.replace(this);
  }
  return *this;
}

void ListElementImpl::unlink() noexcept {
  prev_->next_ = next_;
  next_->prev_ = prev_;
  next_ = prev_ = this;
}

void ListElementImpl::replace(ListElementImpl* replacement) noexcept {
  replacement->unlink();
  if (is_unlinked()) {
    return;
  }
  replacement->prev_ = prev_;
  replacement->next_ = next_;
  prev_->next_ = replacement;
  next_->prev_ = replacement;
  next_ = prev_ = this;
}

bool ListElementImpl::is_unlinked() const noexcept {
  return next_ == this && prev_ == this;
}
} // namespace ct::intrusive::detail
