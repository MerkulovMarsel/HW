#pragma once

#include <utility>

namespace ct::detail::element {
struct ElementBase {
  ElementBase();

  ElementBase(ElementBase* parent, ElementBase* left, ElementBase* right);

  ElementBase(const ElementBase&) = default;

  ElementBase& operator=(const ElementBase&) = default;

  ElementBase(ElementBase&&) noexcept;

  static void reset(ElementBase& ptr);

  bool is_sentinel() const noexcept;

  static bool to_left(ElementBase*& current) noexcept;

  static bool to_right(ElementBase*& current) noexcept;

  static bool to_parent(ElementBase*& current) noexcept;

  bool has_parent() const noexcept;

  bool has_left() const noexcept;

  bool has_right() const noexcept;

  static void
  link_to_parent(ElementBase* parent, ElementBase* child, bool compare_result, bool overwrite = true) noexcept;

  bool operator==(const ElementBase& other) const = default;

  static void unlink(ElementBase* node) noexcept;

  bool is_right_child() const noexcept;

  bool is_left_child() const noexcept;

  friend void swap(ElementBase& lhs, ElementBase& rhs) noexcept;

  ~ElementBase() = default;

  ElementBase* parent_ = nullptr;
  ElementBase* left_ = nullptr;
  ElementBase* right_ = nullptr;
};

template <typename Side>
class ElementTag : public ElementBase {
  using Key = typename Side::Key;

public:
  template <typename KeyValue>
  explicit ElementTag(KeyValue&& value)
      : key_(std::forward<KeyValue>(value)) {}

  Key& get_key() {
    return key_;
  }

private:
  Key key_;
};

template <bool is_left>
struct SideSentinel : ElementBase {
  SideSentinel() {
    reset(*this);
  }
};

struct Sentinel
    : SideSentinel<true>
    , SideSentinel<false> {
  template <typename Side>
  static Sentinel* to_sentinel(ElementBase* ptr) {
    if (!ptr->is_sentinel()) {
      return nullptr;
    }
    return static_cast<Sentinel*>(static_cast<SideSentinel<Side::is_left>*>(ptr));
  }

  template <typename Side>
  ElementBase* to_base() {
    return static_cast<SideSentinel<Side::is_left>*>(this);
  }

  template <typename Side>
  const ElementBase* to_base() const {
    return const_cast<Sentinel*>(this)->to_base<Side>();
  }
};
} // namespace ct::detail::element
