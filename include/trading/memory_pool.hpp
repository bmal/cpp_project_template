#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include "trading/asserts.hpp"

namespace Trading::Core {

template <typename T>
class MemoryPool {
   public:
    explicit MemoryPool(std::size_t numElements)
        : _store(numElements, ObjectBlock{}) {
        static_assert(alignof(ObjectBlock) == alignof(T),
                      "ObjectBlock must be aligned properly for T");
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) noexcept = default;
    MemoryPool& operator=(MemoryPool&&) noexcept = default;

    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) noexcept {
        updateNextFreeIndex();
        auto* block = &_store[_nextFreeIndex];
        ASSERT(block->isFree,
               "Block " + std::to_string(_nextFreeIndex) + " must be free");

        T* obj = new (&block->data) T(std::forward<Args>(args)...);
        block->isFree = false;

        return obj;
    }

    void deallocate(T* obj) noexcept {
        const auto* objBlock = reinterpret_cast<const ObjectBlock*>(obj);
        ASSERT(isValidPoolObject(objBlock), "Object not in pool");

        const auto* storeData = _store.data();
        const auto index = static_cast<std::size_t>(objBlock - storeData);

        ASSERT(!_store[index].isFree,
               "Double free at index " + std::to_string(index));

        obj->~T();
        _store[index].isFree = true;
    }

   private:
    void updateNextFreeIndex() noexcept {
        const auto startIndex = _nextFreeIndex;
        const auto size = _store.size();

        do {
            if (_store[_nextFreeIndex].isFree) {
                return;
            }
            _nextFreeIndex = (_nextFreeIndex + 1) % size;
        } while (_nextFreeIndex != startIndex);

        FATAL("Memory pool exhausted");
    }

    [[nodiscard]] bool isValidPoolObject(const auto* obj) const noexcept {
        const auto* storeData = _store.data();
        return (obj >= storeData && obj < (storeData + _store.size()) &&
                (reinterpret_cast<std::uintptr_t>(obj) -
                 reinterpret_cast<std::uintptr_t>(storeData)) %
                        sizeof(ObjectBlock) ==
                    0);
    }

    struct alignas(T) ObjectBlock {
        std::byte data[sizeof(T)];
        bool isFree{true};
    };

    std::vector<ObjectBlock> _store;
    std::size_t _nextFreeIndex{0};
};

}  // namespace Trading::Core
