#include <gtest/gtest.h>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
#ifdef __APPLE__
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
#endif
    return RUN_ALL_TESTS();
}
