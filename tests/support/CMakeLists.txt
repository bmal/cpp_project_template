# Mechanism: one support library that every test executable links in place of gtest_main.
# An OBJECT library, so the operator new replacement always reaches the link of the executable.
# Builders and FakeClock are header-only and use the headers of the module under test,
# so this target depends on GoogleTest alone and links into any module's tests.
add_library(myproj_test_support OBJECT src/allocation_guard.cpp src/main.cpp)
target_include_directories(myproj_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(
  myproj_test_support
  PUBLIC GTest::gmock
  PRIVATE myproj_warnings myproj_options
)
