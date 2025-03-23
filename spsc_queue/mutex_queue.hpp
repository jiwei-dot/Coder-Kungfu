#pragma once

#include <array>
#include <cstddef>
#include <mutex>

template <typename T, size_t Capacity>
class MutexQueue {
 public:
  MutexQueue() = default;
  MutexQueue(const MutexQueue&) = delete;
  MutexQueue& operator=(const MutexQueue&) = delete;

  bool Push(const T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (write_idx_ - read_idx_ >= Capacity) return false;
    buffer_[write_idx_ % Capacity] = std::move(item);
    write_idx_++;
    return true;
  }

  bool Pop(T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (write_idx_ == read_idx_) return false;
    item = buffer_[read_idx_ % Capacity];
    read_idx_++;
    return true;
  }

 private:
  std::mutex mutex_;
  std::array<T, Capacity> buffer_;
  size_t read_idx_{0};
  size_t write_idx_{0};
};
