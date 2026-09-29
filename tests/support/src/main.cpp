// Mechanism: the one GoogleTest main every test executable shares, in place of gmock_main.
// Test-wide setup, such as a listener or a global environment, belongs here and nowhere else.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    // Death tests re-execute the binary, which stays correct when a test has started threads.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
