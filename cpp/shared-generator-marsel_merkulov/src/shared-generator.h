#pragma once

#include <coroutine>
#include <memory>
#include <mutex>
#include <optional>

namespace ct {

template <typename T>
class SharedGenerator {
  struct SharedGeneratorControlBlock;

  struct SharedGeneratorPromise {
    SharedGenerator get_return_object() noexcept {
      std::shared_ptr<SharedGeneratorControlBlock> cb = std::make_shared<SharedGeneratorControlBlock>(
          std::coroutine_handle<SharedGeneratorPromise>::from_promise(*this)
      );

      return SharedGenerator{std::move(cb)};
    }

    std::suspend_always initial_suspend() noexcept {
      return {};
    }

    std::suspend_always final_suspend() noexcept {
      return {};
    }

    std::suspend_always yield_value(T&& value) noexcept {
      value_ = std::addressof(value);
      return {};
    }

    std::suspend_always yield_value(const T& value) noexcept {
      value_ = std::addressof(value);
      return {};
    }

    void return_void() noexcept {}

    void unhandled_exception() {
      throw;
    }

    std::variant<T*, const T*> value_;
  };

  struct SharedGeneratorControlBlock {
    std::coroutine_handle<SharedGeneratorPromise> handle_;
    std::mutex mutex_;

    SharedGeneratorControlBlock() = default;
    SharedGeneratorControlBlock(SharedGeneratorControlBlock&&) = default;
    SharedGeneratorControlBlock& operator=(SharedGeneratorControlBlock&&) = default;
    SharedGeneratorControlBlock(const SharedGeneratorControlBlock&) = default;
    SharedGeneratorControlBlock& operator=(const SharedGeneratorControlBlock&) = default;

    explicit SharedGeneratorControlBlock(std::coroutine_handle<SharedGeneratorPromise> handle)
        : handle_(handle) {}

    ~SharedGeneratorControlBlock() {
      if (handle_) {
        handle_.destroy();
      }
    }
  };

  explicit SharedGenerator(std::shared_ptr<SharedGeneratorControlBlock> control_block)
      : control_block_{std::move(control_block)} {}

public:
  using promise_type = SharedGeneratorPromise;

  SharedGenerator() = default;

  std::optional<T> next() {
    if (!control_block_) {
      return std::nullopt;
    }
    std::lock_guard lock(control_block_->mutex_);
    if (control_block_->handle_.done()) {
      return std::nullopt;
    }
    control_block_->handle_.resume();
    if (control_block_->handle_.done()) {
      return std::nullopt;
    }

    auto& promise = control_block_->handle_.promise();
    return std::visit(
        []<typename P>(P* ptr) {
          return std::optional<T>{std::forward<P>(*ptr)};
        },
        promise.value_
    );
  }

private:
  std::shared_ptr<SharedGeneratorControlBlock> control_block_;
};

} // namespace ct
