#pragma once

#include "multi-array.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <functional>
#include <thread>

namespace ct {

template <typename State, size_t D>
class GridView;

template <typename State, size_t D = 2>
class GridView {
  using Data = detail::MultiArr<std::remove_cv_t<State>, D>;

  template <typename U, size_t N>
  friend class GridView;

public:
  explicit GridView(Data& data)
      : data(data) {}

  template <typename U>
    requires (std::is_same_v<U, std::remove_cv_t<State>> && std::is_const_v<State>)
  GridView(const GridView<U, D>& other)
      : data(other.data) {}

  size_t extent(size_t dim) const {
    return data.sizes[dim];
  }

  template <typename... Indices>
    requires (sizeof...(Indices) == D && (std::convertible_to<Indices, size_t> && ...))
  State& operator[](Indices... indices) const {
    return data[indices...];
  }

  State& operator[](const std::array<size_t, D>& indices) const {
    return data[indices];
  }

private:
  Data& data;
};

namespace detail {
template <typename State, size_t D, typename Rule>
concept Rules = []<size_t... Is>(std::index_sequence<Is...>) {
  return requires (Rule rule, const GridView<const State, D> grid) {
    { std::invoke(rule, grid, Is...) } -> std::convertible_to<State>;
  };
}(std::make_index_sequence<D>{});
} // namespace detail

template <typename State, typename Rule, size_t D = 2>
  requires detail::Rules<State, D, Rule>
class CellularAutomaton {
  using Data = detail::MultiArr<std::remove_cv_t<State>, D>;
  using View = GridView<State, D>;
  using ConstView = GridView<const State, D>;
  using IndexArray = std::array<size_t, D>;

public:
  CellularAutomaton(const IndexArray& extents, size_t n_threads, Rule rule = {})
      : data(extents)
      , step_new_data(extents)
      , rule(std::move(rule))
      , n_threads(n_threads)
      , threads_done(n_threads, false) {
    threads.reserve(n_threads);
    for (size_t thread_id = 0; thread_id < n_threads; ++thread_id) {
      threads.emplace_back([this, thread_id, n_threads](std::stop_token token) {
        while (true) {
          std::unique_lock lock(mutex);
          start_cv.wait(lock, token, [this, thread_id]() {
            return step_in_progress && !threads_done[thread_id];
          });

          if (token.stop_requested()) {
            return;
          }
          lock.unlock();
          size_t thread_count = (data.data.size() + n_threads - 1) / n_threads;
          size_t thread_end = std::min(data.data.size(), thread_count * (thread_id + 1));
          for (size_t i = thread_count * thread_id; i < thread_end; ++i) {
            IndexArray indices = data.from_primary(i);
            step_new_data.data[i] = rule_call(indices, std::make_index_sequence<D>());
          }
          lock.lock();
          threads_done[thread_id] = true;
          threads_completed++;

          if (threads_completed == n_threads) {
            step_in_progress = false;
            lock.unlock();
            done_cv.notify_one();
          } else {
            lock.unlock();
          }
        }
      });
    }
  }

  CellularAutomaton(const CellularAutomaton&) = delete;
  CellularAutomaton& operator=(const CellularAutomaton&) = delete;

  GridView<State, D> grid() noexcept {
    return View(data);
  }

  GridView<const State, D> grid() const noexcept {
    return ConstView(data);
  }

  void step() {
    std::unique_lock lock(mutex);
    threads_completed = 0;
    std::fill(threads_done.begin(), threads_done.end(), false);
    step_in_progress = true;
    lock.unlock();
    start_cv.notify_all();
    lock.lock();
    done_cv.wait(lock, [this]() {
      return !step_in_progress;
    });
    data.data.swap(step_new_data.data);
  }

  ~CellularAutomaton() {
    for (auto& thread : threads) {
      thread.request_stop();
    }
    start_cv.notify_all();
  }

private:
  template <size_t... Indices>
    requires (sizeof...(Indices) == D)
  State rule_call(const IndexArray& indices, std::index_sequence<Indices...>) {
    return std::invoke(rule, View(data), indices[Indices]...);
  }

  std::vector<std::jthread> threads;
  Data data;
  Data step_new_data;
  Rule rule;
  size_t n_threads;

  std::vector<bool> threads_done;
  size_t threads_completed = 0;
  bool step_in_progress = false;
  std::condition_variable_any start_cv;
  std::condition_variable done_cv;
  std::mutex mutex;
};

} // namespace ct
