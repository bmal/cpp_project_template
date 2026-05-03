# Create functional test executable
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${TEMPLATE_TEST_TARGET} ${TEST_SOURCES})

target_link_libraries(${TEMPLATE_TEST_TARGET} PRIVATE
    GTest::gtest
    GTest::gmock
    ${TEMPLATE_NAMESPACED_TARGET}
)

# Configure compiler flags
target_compile_features(${TEMPLATE_TEST_TARGET} PRIVATE cxx_std_23)
set_target_properties(
    ${TEMPLATE_TEST_TARGET}
    PROPERTIES
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(
    TARGET ${TEMPLATE_TEST_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TEMPLATE_TEST_TARGET}>
            ${CMAKE_BINARY_DIR}/bin/tests
    COMMENT "Updating stable test launcher"
)

if(NOT TEST_INSTALLED_VERSION)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${TEMPLATE_TEST_TARGET} PRIVATE -Wall -Wpedantic -Wextra -Werror)
    elseif(MSVC)
        target_compile_options(${TEMPLATE_TEST_TARGET} PRIVATE /W4 /WX)
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(${TEMPLATE_TEST_TARGET})

# Handle sanitizer suppressions
if(USE_SANITIZER MATCHES "([Mm]emory.*)")
    target_compile_options(${TEMPLATE_TEST_TARGET} PRIVATE -fno-sanitize=memory)
    target_link_options(${TEMPLATE_TEST_TARGET} PRIVATE -fno-sanitize=memory)
endif()

# Handle code coverage
if(ENABLE_COVERAGE)
    enable_coverage(${TEMPLATE_TEST_TARGET})
endif()

if(USE_SANITIZER)
    if(USE_SANITIZER MATCHES "([Aa]ddress)")
        target_compile_definitions(${TEMPLATE_TEST_TARGET} PRIVATE __SANITIZE_ADDRESS__=1)
    endif()
    if(USE_SANITIZER MATCHES "([Tt]hread)")
        target_compile_definitions(${TEMPLATE_TEST_TARGET} PRIVATE __SANITIZE_THREAD__=1)
    endif()
    if(USE_SANITIZER MATCHES "([Mm]emory)")
        target_compile_definitions(${TEMPLATE_TEST_TARGET} PRIVATE __SANITIZE_MEMORY__=1)
    endif()
    if(USE_SANITIZER MATCHES "([Uu]ndefined)")
        target_compile_definitions(${TEMPLATE_TEST_TARGET} PRIVATE __SANITIZE_UNDEFINED__=1)
    endif()
endif()
