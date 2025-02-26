#include <gtest/gtest.h>
#include <cstddef>
#include <string>
#include "trading/spsc_queue.hpp"

class Test : public ::testing::Test {
   protected:
    static constexpr size_t DEFAULT_QUEUE_CAPACITY = 3;
};

TEST_F(Test, WhenEmptyQueueShouldReturnEmptyStatus) {
    Trading::Core::SPSCQueue<int> queue(DEFAULT_QUEUE_CAPACITY);
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_EQ(queue.size(), 0);
}

TEST_F(Test, WhenPushingToEmptyQueueShouldSucceed) {
    Trading::Core::SPSCQueue<int> queue(DEFAULT_QUEUE_CAPACITY);
    EXPECT_TRUE(queue.tryPush(42));
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_EQ(queue.size(), 1);
}

TEST_F(Test, WhenPoppingFromEmptyQueueShouldNotUpdatePassedVariable) {
    Trading::Core::SPSCQueue<int> queue(DEFAULT_QUEUE_CAPACITY);
    int value = 4;
    EXPECT_FALSE(queue.tryPop(value));
    EXPECT_EQ(value, 4);  // Value should remain unchanged
}

TEST_F(Test, WhenPushingAndPoppingOneShouldMaintainFIFO) {
    Trading::Core::SPSCQueue<int> queue(DEFAULT_QUEUE_CAPACITY);
    const int testValue = 42;
    EXPECT_TRUE(queue.tryPush(testValue));
    int poppedValue = 0;
    EXPECT_TRUE(queue.tryPop(poppedValue));
    EXPECT_EQ(poppedValue, testValue);
    EXPECT_TRUE(queue.isEmpty());
}

TEST_F(Test, WhenQueueIsFullShouldRejectPush) {
    Trading::Core::SPSCQueue<int> queue(1);
    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_FALSE(queue.tryPush(2));
}

TEST_F(Test, WhenQueueEmptiedShouldAllowNewPush) {
    Trading::Core::SPSCQueue<int> queue(1);
    int value = 0;

    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_FALSE(queue.tryPush(2));

    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(value, 1);

    EXPECT_TRUE(queue.tryPush(3));
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(value, 3);
}

TEST_F(Test, WhenQueueHasMultipleElementsShouldMaintainFIFO) {
    Trading::Core::SPSCQueue<int> queue(3);

    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_TRUE(queue.tryPush(2));
    EXPECT_TRUE(queue.tryPush(3));

    // Verify FIFO order
    int value = 0;
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(value, 3);
}

struct TestStruct {
    std::string str;
    int num;

    bool operator==(const TestStruct& other) const {
        return str == other.str && num == other.num;
    }
};

TEST_F(Test, ShouldHandleComplexTypes) {
    Trading::Core::SPSCQueue<TestStruct> queue(DEFAULT_QUEUE_CAPACITY);
    TestStruct input{.str = "test", .num = 42};
    TestStruct output;

    EXPECT_TRUE(queue.tryPush(input));
    EXPECT_TRUE(queue.tryPop(output));
    EXPECT_EQ(output, input);
}

class SPSCQueueCapacityTest : public ::testing::TestWithParam<size_t> {};

TEST_P(SPSCQueueCapacityTest, WhenFillingQueueShouldMaintainFIFOOrder) {
    const size_t capacity = GetParam();
    Trading::Core::SPSCQueue<int> queue(capacity);

    // Fill to capacity
    for (size_t i = 0; i < capacity; ++i) {
        EXPECT_TRUE(queue.tryPush(static_cast<int>(i)));
    }

    // Verify full behavior
    EXPECT_FALSE(queue.tryPush(static_cast<int>(capacity)));

    // Verify FIFO order
    for (size_t i = 0; i < capacity; ++i) {
        int value = -1;
        EXPECT_TRUE(queue.tryPop(value));
        EXPECT_EQ(value, static_cast<int>(i));
    }

    EXPECT_TRUE(queue.isEmpty());
}

INSTANTIATE_TEST_SUITE_P(ValidCapacities,
                         SPSCQueueCapacityTest,
                         ::testing::Values(1, 3, 7, 15));
