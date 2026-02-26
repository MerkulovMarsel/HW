#pragma once

#include <cstddef>
#include <memory>
#include <utility>

namespace ct {
namespace detail {
class ControlBlockBase {
public:
  virtual ~ControlBlockBase() = default;

  ControlBlockBase() = default;

  void add_strong_ref() noexcept {
    ++strong_refs_;
  }

  void add_weak_ref() noexcept {
    ++weak_refs_;
  }

  void release_strong_ref() noexcept {
    --strong_refs_;
    if (strong_refs_ == 0) {
      delete_data();
      release_weak_ref();
    }
  }

  void release_weak_ref() noexcept {
    --weak_refs_;
    if (strong_refs_ == 0 && weak_refs_ == 0) {
      delete this;
    }
  }

  [[nodiscard]] bool is_empty() const noexcept {
    return strong_refs_ == 0;
  }

  std::size_t strong_refs() const noexcept {
    return strong_refs_;
  }

protected:
  virtual void delete_data() noexcept = 0;

private:
  std::size_t strong_refs_ = 1;
  std::size_t weak_refs_ = 1;
};

template <typename T, typename Deleter>
class RegularControlBlock final : public ControlBlockBase {
protected:
  void delete_data() noexcept override {
    deleter_(data_);
  }

public:
  RegularControlBlock(T* data, Deleter deleter)
      : data_(data)
      , deleter_(std::move(deleter)) {}

private:
  T* data_ = nullptr;
  [[no_unique_address]] Deleter deleter_;
};

template <typename T, typename... Args>
class InplaceControlBlock final : public ControlBlockBase {
protected:
  void delete_data() noexcept override {
    data_.~T();
  }

public:
  explicit InplaceControlBlock(Args&&... args)
      : data_(std::forward<Args>(args)...) {}

  T& get_data() {
    return data_;
  }

  ~InplaceControlBlock() noexcept override {}

private:
  union {
    T data_;
  };
};
} // namespace detail

template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
public:
  template <typename Y>
  friend class SharedPtr;

  template <typename Y>
  friend class WeakPtr;

  template <typename Y, typename... Args>
  friend SharedPtr<Y> make_shared(Args&&... args);

  using ControlBlock = detail::ControlBlockBase;

  SharedPtr() noexcept = default;

  SharedPtr(std::nullptr_t) noexcept
      : data_(nullptr) {}

  template <typename Y, typename Deleter = std::default_delete<Y>>
    requires std::is_convertible_v<Y*, T*> && (!std::is_same_v<Y, std::nullptr_t>)
  explicit SharedPtr(Y* ptr, Deleter deleter = {}) try
      : control_block_(new detail::RegularControlBlock<Y, Deleter>(ptr, std::move(deleter)))
      , data_(ptr) {
  } catch (...) {
    deleter(ptr);
    throw;
  }

  template <typename Y>
  SharedPtr(const SharedPtr<Y>& other, T* ptr) noexcept
      : control_block_(other.control_block_)
      , data_(ptr) {
    add_ref();
  }

  template <typename Y>
  SharedPtr(SharedPtr<Y>&& other, T* ptr) noexcept
      : control_block_(other.control_block_)
      , data_(ptr) {
    other.control_block_ = nullptr;
    other.data_ = nullptr;
  }

  SharedPtr(const SharedPtr& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    add_ref();
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr(const SharedPtr<Y>& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    add_ref();
  }

  SharedPtr(SharedPtr&& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    other.control_block_ = nullptr;
    other.data_ = nullptr;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr(SharedPtr<Y>&& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    other.control_block_ = nullptr;
    other.data_ = nullptr;
  }

  SharedPtr& operator=(const SharedPtr& other) noexcept {
    if (this != &other) {
      SharedPtr temp(other);
      swap(temp);
    }
    return *this;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr& operator=(const SharedPtr<Y>& other) noexcept {
    SharedPtr temp(other);
    swap(temp);
    return *this;
  }

  SharedPtr& operator=(SharedPtr&& other) noexcept {
    if (this != &other) {
      if (control_block_) {
        control_block_->release_strong_ref();
      }
      control_block_ = other.control_block_;
      data_ = other.data_;
      other.control_block_ = nullptr;
      other.data_ = nullptr;
    }
    return *this;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr& operator=(SharedPtr<Y>&& other) noexcept {
    if (control_block_) {
      control_block_->release_strong_ref();
    }
    control_block_ = other.control_block_;
    data_ = other.data_;
    other.control_block_ = nullptr;
    other.data_ = nullptr;
    return *this;
  }

  T* get() const noexcept {
    return data_;
  }

  template <typename Y>
    requires std::is_convertible_v<T*, Y*>
  explicit operator SharedPtr<Y>() const {
    return SharedPtr<Y>(*this);
  }

  explicit operator bool() const noexcept {
    return get() != nullptr;
  }

  T& operator*() const noexcept {
    return *data_;
  }

  T* operator->() const noexcept {
    return data_;
  }

  std::size_t use_count() const noexcept {
    return control_block_ ? control_block_->strong_refs() : 0;
  }

  void reset() noexcept {
    if (control_block_) {
      control_block_->release_strong_ref();
      control_block_ = nullptr;
      data_ = nullptr;
    }
  }

  template <typename Y>
  void reset(Y* new_ptr) {
    SharedPtr temp(new_ptr);
    swap(temp);
  }

  template <typename Y, typename Deleter>
    requires std::is_convertible_v<Y*, T*>
  void reset(Y* new_ptr, Deleter deleter) {
    SharedPtr temp(new_ptr, std::move(deleter));
    swap(temp);
  }

  void swap(SharedPtr& other) noexcept {
    std::swap(control_block_, other.control_block_);
    std::swap(data_, other.data_);
  }

  friend bool operator==(const SharedPtr& lhs, const SharedPtr& rhs) noexcept {
    return lhs.get() == rhs.get();
  }

  friend bool operator!=(const SharedPtr& lhs, const SharedPtr& rhs) noexcept {
    return !(lhs == rhs);
  }

  template <typename U, typename V>
  friend bool operator==(const SharedPtr<U>& lhs, const SharedPtr<V>& rhs) noexcept;

  template <typename U, typename V>
  friend bool operator!=(const SharedPtr<U>& lhs, const SharedPtr<V>& rhs) noexcept;

  ~SharedPtr() {
    if (control_block_) {
      control_block_->release_strong_ref();
    }
  }

private:
  template <typename... Args>
  explicit SharedPtr(detail::InplaceControlBlock<T, Args...>* control_block)
      : control_block_(control_block)
      , data_(&control_block->get_data()) {}

  explicit SharedPtr(ControlBlock* control_block, T* data)
      : control_block_(control_block)
      , data_(data) {
    add_ref();
  }

  void add_ref() noexcept {
    if (control_block_) {
      control_block_->add_strong_ref();
    }
  }

  ControlBlock* control_block_ = nullptr;
  T* data_ = nullptr;
};

template <typename T>
class WeakPtr {
public:
  template <typename Y>
  friend class SharedPtr;

  template <typename Y>
  friend class WeakPtr;

  WeakPtr() noexcept = default;

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  WeakPtr(const SharedPtr<Y>& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    add_ref();
  }

  WeakPtr(const WeakPtr& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    add_ref();
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  WeakPtr(const WeakPtr<Y>& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    add_ref();
  }

  WeakPtr(WeakPtr&& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    other.control_block_ = nullptr;
    other.data_ = nullptr;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  WeakPtr(WeakPtr<Y>&& other) noexcept
      : control_block_(other.control_block_)
      , data_(other.data_) {
    other.control_block_ = nullptr;
    other.data_ = nullptr;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  WeakPtr& operator=(const SharedPtr<Y>& other) noexcept {
    WeakPtr temp(other);
    swap(temp);
    return *this;
  }

  WeakPtr& operator=(const WeakPtr& other) noexcept {
    WeakPtr temp(other);
    swap(temp);
    return *this;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  WeakPtr& operator=(const WeakPtr<Y>& other) noexcept {
    WeakPtr temp(other);
    swap(temp);
    return *this;
  }

  WeakPtr& operator=(WeakPtr&& other) noexcept {
    if (this != &other) {
      if (control_block_) {
        control_block_->release_weak_ref();
      }
      control_block_ = other.control_block_;
      data_ = other.data_;
      other.control_block_ = nullptr;
      other.data_ = nullptr;
    }
    return *this;
  }

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  WeakPtr& operator=(WeakPtr<Y>&& other) noexcept {
    if (control_block_) {
      control_block_->release_weak_ref();
    }
    control_block_ = other.control_block_;
    data_ = other.data_;
    other.control_block_ = nullptr;
    other.data_ = nullptr;
    return *this;
  }

  SharedPtr<T> lock() const noexcept {
    if (control_block_ && !control_block_->is_empty()) {
      return SharedPtr<T>(control_block_, data_);
    }
    return SharedPtr<T>();
  }

  void reset() noexcept {
    if (control_block_) {
      control_block_->release_weak_ref();
      control_block_ = nullptr;
      data_ = nullptr;
    }
  }

  void swap(WeakPtr& other) noexcept {
    std::swap(control_block_, other.control_block_);
    std::swap(data_, other.data_);
  }

  ~WeakPtr() {
    if (control_block_) {
      control_block_->release_weak_ref();
    }
  }

private:
  void add_ref() noexcept {
    if (control_block_) {
      control_block_->add_weak_ref();
    }
  }

  detail::ControlBlockBase* control_block_ = nullptr;
  T* data_ = nullptr;
};

template <typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {
  auto* control_block = new detail::InplaceControlBlock<T, Args...>(std::forward<Args>(args)...);
  return SharedPtr<T>(control_block);
}

template <typename T, typename U>
bool operator==(const SharedPtr<T>& lhs, const SharedPtr<U>& rhs) noexcept {
  return lhs.get() == rhs.get();
}

template <typename T, typename U>
bool operator!=(const SharedPtr<T>& lhs, const SharedPtr<U>& rhs) noexcept {
  return !(lhs == rhs);
}

template <typename T, typename U>
bool operator==(const SharedPtr<T>& lhs, const WeakPtr<U>& rhs) {
  return lhs == rhs.lock();
}

template <typename T, typename U>
bool operator==(const WeakPtr<T>& lhs, const SharedPtr<U>& rhs) {
  return lhs.lock() == rhs;
}

template <typename T, typename U>
bool operator!=(const SharedPtr<T>& lhs, const WeakPtr<U>& rhs) {
  return !(lhs == rhs);
}

template <typename T, typename U>
bool operator!=(const WeakPtr<T>& lhs, const SharedPtr<U>& rhs) {
  return !(lhs == rhs);
}
} // namespace ct
