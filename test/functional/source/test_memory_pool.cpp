#include <gtest/gtest.h>
#include <array>
#include <string>
#include <utility>
#include "sanitizer_suppressions.hpp"
#include "trading/memory_pool.hpp"

class SimpleObject {
   public:
    explicit SimpleObject(int val) : value(val) {}
    int value;
};

class MemoryPoolTest : public testing::Test {};

TEST_F(MemoryPoolTest, BasicAllocation) {
    Trading::Core::MemoryPool<SimpleObject> pool(1);
    auto* obj = pool.allocate(42);

    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);
}

TEST_F(MemoryPoolTest, BasicDeallocation) {
    Trading::Core::MemoryPool<SimpleObject> pool(1);
    auto* obj = pool.allocate(42);
    EXPECT_NO_THROW(pool.deallocate(obj));
}

TEST_F(MemoryPoolTest, ConstructorArguments) {
    Trading::Core::MemoryPool<std::string> pool(1);
    auto* str = pool.allocate("test");
    EXPECT_EQ(*str, "test");
    pool.deallocate(str);
}

TEST_F(MemoryPoolTest, PoolExhaustion) {
    Trading::Core::MemoryPool<SimpleObject> pool(2);
    auto* obj1 = pool.allocate(1);
    auto* obj2 = pool.allocate(2);

    ASSERT_NE(obj1, nullptr);
    ASSERT_NE(obj2, nullptr);
    EXPECT_DEATH((void)pool.allocate(3), ".*");
}

TEST_F(MemoryPoolTest, DoubleFreeDetection) {
    Trading::Core::MemoryPool<SimpleObject> pool(1);
    auto* obj = pool.allocate(42);
    pool.deallocate(obj);
    EXPECT_DEBUG_DEATH(pool.deallocate(obj), ".*");
}

TEST_F(MemoryPoolTest, InvalidPointerDetection) {
    SKIP_UNDER_SANITIZER(ASAN, "Test intentionally uses invalid memory access");

    Trading::Core::MemoryPool<SimpleObject> pool(1);
    SimpleObject standalone(42);
    EXPECT_DEBUG_DEATH(pool.deallocate(&standalone), ".*");
}

TEST_F(MemoryPoolTest, ReuseAfterFree) {
    Trading::Core::MemoryPool<SimpleObject> pool(1);

    auto* obj1 = pool.allocate(1);
    EXPECT_EQ(obj1->value, 1);
    pool.deallocate(obj1);

    auto* obj2 = pool.allocate(2);
    EXPECT_EQ(obj2->value, 2);
    pool.deallocate(obj2);
}

TEST_F(MemoryPoolTest, MoveOperations) {
    Trading::Core::MemoryPool<SimpleObject> pool1(1);
    auto* obj = pool1.allocate(42);

    auto pool2 = std::move(pool1);
    EXPECT_NO_THROW(pool2.deallocate(obj));
}

TEST_F(MemoryPoolTest, ObjectLifetime) {
    static int constructorCalls = 0;
    static int destructorCalls = 0;

    struct LifetimeTracker {
        LifetimeTracker() { ++constructorCalls; }
        ~LifetimeTracker() { ++destructorCalls; }
    };

    {
        Trading::Core::MemoryPool<LifetimeTracker> pool(1);
        auto* obj = pool.allocate();
        pool.deallocate(obj);
    }

    EXPECT_EQ(constructorCalls, 1);
    EXPECT_EQ(destructorCalls, 1);
}

TEST_F(MemoryPoolTest, SmallType) {
    Trading::Core::MemoryPool<char> pool(1);
    auto* obj = pool.allocate('a');
    EXPECT_EQ(*obj, 'a');
    pool.deallocate(obj);
}

TEST_F(MemoryPoolTest, LargeType) {
    Trading::Core::MemoryPool<std::array<int, 1000>> pool(1);
    auto* obj = pool.allocate();
    ASSERT_NE(obj, nullptr);
    pool.deallocate(obj);
}

TEST_F(MemoryPoolTest, FullCycle) {
    Trading::Core::MemoryPool<SimpleObject> pool(3);

    auto* obj1 = pool.allocate(1);
    auto* obj2 = pool.allocate(2);
    auto* obj3 = pool.allocate(3);

    pool.deallocate(obj2);
    pool.deallocate(obj1);
    pool.deallocate(obj3);

    obj1 = pool.allocate(4);
    obj2 = pool.allocate(5);
    obj3 = pool.allocate(6);

    EXPECT_EQ(obj1->value, 4);
    EXPECT_EQ(obj2->value, 5);
    EXPECT_EQ(obj3->value, 6);
}
