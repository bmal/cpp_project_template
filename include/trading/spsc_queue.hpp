#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>

namespace Trading::Core {

template <typename T>
class SPSCQueue {
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "Platform must support lock-free atomics");

    static_assert(std::is_trivially_copyable_v<T> ||
                      std::is_copy_constructible_v<T>,
                  "T must be trivially copyable or copy constructible");

   public:
    explicit SPSCQueue(
        size_t minimalCapacity)  // TODO fix for use case when maximalCapacity
                                 // is equal to maximal value of size_t
        : _nextWriterIndex{},
          _nextReaderIndex{},
          CAPACITY_MASK(nextPowerOfTwo(minimalCapacity + 1) - 1),
          _store(std::make_unique<T[]>(CAPACITY_MASK + 1)) {}

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    [[nodiscard]] bool tryPush(const T& value) noexcept {
        const size_t writePos =
            _nextWriterIndex.value.load(std::memory_order_relaxed);
        const size_t readPos =
            _nextReaderIndex.value.load(std::memory_order_acquire);

        if (((writePos + 1) & CAPACITY_MASK) == readPos) {
            return false;  // Full
        }

        _store[writePos] = value;
        _nextWriterIndex.value.store((writePos + 1) & CAPACITY_MASK,
                                     std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool tryPop(T& value) noexcept {
        const size_t readPos =
            _nextReaderIndex.value.load(std::memory_order_relaxed);
        const size_t writePos =
            _nextWriterIndex.value.load(std::memory_order_acquire);

        if (readPos == writePos) {
            return false;  // Empty
        }

        value = _store[readPos];
        _nextReaderIndex.value.store((readPos + 1) & CAPACITY_MASK,
                                     std::memory_order_release);
        return true;
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t writePos =
            _nextWriterIndex.value.load(std::memory_order_acquire);
        const size_t readPos =
            _nextReaderIndex.value.load(std::memory_order_acquire);
        return (writePos + CAPACITY_MASK + 1 - readPos) & CAPACITY_MASK;
    }

    [[nodiscard]] bool isEmpty() const noexcept {
        return _nextWriterIndex.value.load(std::memory_order_acquire) ==
               _nextReaderIndex.value.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t capacity() const noexcept { return CAPACITY_MASK; }

   private:
    static constexpr size_t nextPowerOfTwo(size_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        return v + 1;
    }

    struct alignas(64) PaddedAtomic {
        std::atomic<size_t> value{0};
        char padding[64 - sizeof(std::atomic<size_t>)]{};
    };

    PaddedAtomic _nextWriterIndex;
    PaddedAtomic _nextReaderIndex;
    const size_t CAPACITY_MASK;
    std::unique_ptr<T[]> _store;
};

}  // namespace Trading::Core
