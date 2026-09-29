// Mechanism: counts heap allocations for AllocationGuard without hiding bugs from sanitizers.
// Plain builds replace every global operator new and delete; ASan, TSan, and MSan builds use their hook.
#include "support/allocation_guard.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>

// Darwin sanitizers have no allocation hook, and ASan there ignores mismatched new and delete.
#if defined(__APPLE__)
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define MYPROJ_SANITIZER_ALLOCATOR 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) ||                      \
    __has_feature(memory_sanitizer)
#define MYPROJ_SANITIZER_ALLOCATOR 1
#endif
#endif

namespace {

std::atomic<std::size_t> allocation_count{0};

} // namespace

#if defined(MYPROJ_SANITIZER_ALLOCATOR)

#include <sanitizer/allocator_interface.h>

// The sanitizer keeps its own operator new so it still reports new[] freed with delete.
// Its hook sees every heap allocation, including malloc.
namespace {

void count_allocation(const volatile void*, std::size_t) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
}

// The runtime refuses a null hook, so frees get one that does nothing.
void ignore_free(const volatile void*) {}

const bool counting = __sanitizer_install_malloc_and_free_hooks(count_allocation, ignore_free) != 0;

} // namespace

#else

#include <cstdint>
#include <cstdlib>
#include <new>

namespace {

void* counted_malloc(std::size_t size) noexcept {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    // malloc(0) may return null, but operator new must return a unique pointer.
    return std::malloc(size == 0 ? 1 : size);
}

void* counted_aligned_alloc(std::size_t size, std::align_val_t align) noexcept {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    const auto alignment = static_cast<std::size_t>(align);
    if (size > SIZE_MAX - alignment) {
        return nullptr;
    }
    // aligned_alloc needs a nonzero size that is a multiple of the alignment.
    const std::size_t rounded = size == 0 ? alignment : (size + alignment - 1) / alignment * alignment;
    return std::aligned_alloc(alignment, rounded);
}

constexpr bool counting = true;

void* throw_if_null(void* pointer) {
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}

} // namespace

// Each new pairs with a delete defined here, so no allocation crosses to another allocator.
void* operator new(std::size_t size) { return throw_if_null(counted_malloc(size)); }
void* operator new[](std::size_t size) { return throw_if_null(counted_malloc(size)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return counted_malloc(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return counted_malloc(size); }
void* operator new(std::size_t size, std::align_val_t align) {
    return throw_if_null(counted_aligned_alloc(size, align));
}
void* operator new[](std::size_t size, std::align_val_t align) {
    return throw_if_null(counted_aligned_alloc(size, align));
}
void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return counted_aligned_alloc(size, align);
}
void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    return counted_aligned_alloc(size, align);
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

#endif

namespace myproj::test_support {

std::size_t allocations_so_far() noexcept {
    return allocation_count.load(std::memory_order_relaxed);
}

AllocationGuard::AllocationGuard(std::source_location where) noexcept
    : where_(where), start_(allocations_so_far()) {}

AllocationGuard::~AllocationGuard() {
    const std::size_t count = allocations();
    if (!counting) {
        ADD_FAILURE_AT(where_.file_name(), static_cast<int>(where_.line()))
            << "AllocationGuard: the sanitizer refused the allocation hook, so nothing was counted";
    } else if (count != 0) {
        ADD_FAILURE_AT(where_.file_name(), static_cast<int>(where_.line()))
            << "AllocationGuard: " << count << " allocation(s) in a scope that must not allocate";
    }
}

std::size_t AllocationGuard::allocations() const noexcept {
    return allocations_so_far() - start_;
}

} // namespace myproj::test_support
