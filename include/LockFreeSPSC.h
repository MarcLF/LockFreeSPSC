#pragma once
#include <atomic>
#include <new> // For std::hardware_destructive_interference_size
#include <optional>

// Determine cache line size (usually 64 bytes)
#ifdef __cpp_lib_hardware_interference_size
constexpr size_t cache_line = std::hardware_destructive_interference_size;
#else
constexpr size_t cache_line = 64;
#endif

template<typename T, size_t Capacity>
class LockFreeSPSC // NOLINT(clang-diagnostic-padded)
{
    // +1 to distinguish full vs empty
    static constexpr size_t buffer_size = Capacity + 1;

    // Data
    // Align buffer to avoid sharing cache lines with adjacent objects
    alignas(cache_line) T buffer_[buffer_size];

    // Consumer owned
    alignas(cache_line) std::atomic<size_t> head_{ 0 };
    // We keep a shadow copy of tail here to avoid loading the atomic one
    // constantly
    size_t cached_tail_{ 0 };

    // Producer owned
    alignas(cache_line) std::atomic<size_t> tail_{ 0 };
    // Shadow copy of head
    size_t cached_head_{ 0 };

    static size_t next_index(size_t current_idx);

  public:
    // Producer only
    bool push(const T& item);

    // Consumer only
    std::optional<T> pop();

    // Basic checks
    bool empty() const;
    bool full() const;
};

template<typename T, size_t Capacity>
size_t LockFreeSPSC<T, Capacity>::next_index(const size_t current_idx)
{
    return (current_idx + 1) % buffer_size;
}

template<typename T, size_t Capacity>
bool LockFreeSPSC<T, Capacity>::push(const T& item)
{
    const size_t current_tail = tail_.load(std::memory_order_relaxed);
    const size_t next_tail = next_index(current_tail);

    // Check against our cached copy first (cheap check)
    if (next_tail == cached_head_)
    {
        // Refresh our cache from the atomic variable (expensive check)
        cached_head_ = head_.load(std::memory_order_acquire);

        // Is it still full?
        if (next_tail == cached_head_)
        {
            return false;
        }
    }

    buffer_[current_tail] = item;
    tail_.store(next_tail, std::memory_order_release);
    return true;
}

template<typename T, size_t Capacity>
std::optional<T> LockFreeSPSC<T, Capacity>::pop()
{
    const size_t current_head = head_.load(std::memory_order_relaxed);

    // Check against our cached copy first (cheap check)
    if (current_head == cached_tail_)
    {
        // Refresh cache (expensive check)
        cached_tail_ = tail_.load(std::memory_order_acquire);

        // Still empty?
        if (current_head == cached_tail_)
        {
            return std::nullopt;
        }
    }

    T item = buffer_[current_head];
    head_.store(next_index(current_head), std::memory_order_release);
    return item;
}

template<typename T, size_t Capacity>
bool LockFreeSPSC<T, Capacity>::empty() const
{
    return head_.load(std::memory_order_acquire) ==
           tail_.load(std::memory_order_acquire);
}
template<typename T, size_t Capacity>
bool LockFreeSPSC<T, Capacity>::full() const
{
    return next_index(tail_.load(std::memory_order_acquire)) ==
           head_.load(std::memory_order_acquire);
}
