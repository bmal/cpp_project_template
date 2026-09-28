# Collect source files
file(GLOB_RECURSE BENCHMARK_SOURCES "source/*.cpp")

# Create benchmark executable
add_executable(${TEMPLATE_BENCHMARK_TARGET} ${BENCHMARK_SOURCES})

set_target_properties(
    ${TEMPLATE_BENCHMARK_TARGET}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(
    TARGET ${TEMPLATE_BENCHMARK_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TEMPLATE_BENCHMARK_TARGET}>
            ${CMAKE_BINARY_DIR}/bin/benchmarks
    COMMENT "Updating stable benchmark launcher"
)

# Link dependencies
target_link_libraries(${TEMPLATE_BENCHMARK_TARGET}
    PRIVATE
        ${TEMPLATE_NAMESPACED_TARGET}
        benchmark::benchmark
        myproj_warnings
        myproj_options
)

add_custom_target(
  run-benchmarks
  COMMAND $<TARGET_FILE:${TEMPLATE_BENCHMARK_TARGET}>
  DEPENDS ${TEMPLATE_BENCHMARK_TARGET}
  COMMENT "Running ${TEMPLATE_BENCHMARK_TARGET}"
)
