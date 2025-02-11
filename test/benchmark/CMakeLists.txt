# Set benchmark target name
set(BENCHMARK_TARGET "TradingBenchmarks")

# Collect source files
file(GLOB_RECURSE BENCHMARK_SOURCES "source/*.cpp")

# Create benchmark executable
add_executable(${BENCHMARK_TARGET} ${BENCHMARK_SOURCES})

# Fix include paths - use PROJECT_SOURCE_DIR to get the root of the project
target_include_directories(${BENCHMARK_TARGET}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/../include  # Go up one level from test dir to main project
)

# Link dependencies
target_link_libraries(${BENCHMARK_TARGET}
    PRIVATE
        Trading
        benchmark::benchmark
)

# Set compiler flags
target_compile_features(${BENCHMARK_TARGET} PRIVATE cxx_std_23)
target_compile_options(${BENCHMARK_TARGET}
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -Wall
            -Wextra
            -Werror
            -Wconversion
            -Wsign-conversion
        >
)
