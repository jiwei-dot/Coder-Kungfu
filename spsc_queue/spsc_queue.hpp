#pragma once

#include <array>
#include <atomic>
#include <cstddef>

template <typename T, size_t Capacity>
class SPSCQueue {
 public:
  SPSCQueue() = default;
  SPSCQueue(const SPSCQueue&) = delete;
  SPSCQueue& operator=(const SPSCQueue&) = delete;

  // Read read_idx_, Modify write_idx_
  bool Push(const T& item) {
    const size_t current_write = write_idx_.load(std::memory_order_relaxed);
    const size_t next_write = current_write + 1;
    if (Full())
        return false;

    // placement new
    buffer_[current_write & kMask] = std::move(item);
    write_idx_.store(next_write, std::memory_order_release);
    return true;
  }

  // Modify read_idx_, Read write_idx_
  bool Pop(T& item) {
    const size_t current_read = read_idx_.load(std::memory_order_relaxed);
    const size_t next_read = current_read + 1;
    if (Empty())
        return false;

    // release memory ?
    item = buffer_[current_read & kMask];
    read_idx_.store(next_read, std::memory_order_release);
    return true;
  }

private:
  // For producer only
  bool Full() const noexcept {
    const size_t current_write = write_idx_.load(std::memory_order_relaxed);
    const size_t current_read = read_idx_.load(std::memory_order_acquire);
    return current_write - current_read == Capacity;
  }

  // For consumer only
  bool Empty() const noexcept {
    const size_t current_read = read_idx_.load(std::memory_order_relaxed);
    const size_t current_write = write_idx_.load(std::memory_order_acquire);
    return current_write == current_read;
  }

  constexpr size_t GetCapacity() const noexcept { return Capacity; }

 private:
  // 00010000 & 00001111 = 0
  static_assert(((Capacity >= 2) && (Capacity & (Capacity - 1) == 0)),
                "Capacity must be power of two and at least two");
  alignas(64) std::atomic<size_t> write_idx_{0};
  alignas(64) std::atomic<size_t> read_idx_{0};
  std::array<T, Capacity> buffer_;
  static constexpr size_t kMask = Capacity - 1;
};