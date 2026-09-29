# Mechanism: one support library that every benchmark executable links in place of benchmark_main.
# An OBJECT library, so its main always reaches the link of the executable.
add_library(
  myproj_bench_support
  OBJECT
  src/cache_flush.cpp
  src/main.cpp
  src/percentiles.cpp
)
target_include_directories(myproj_bench_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(
  myproj_bench_support
  PUBLIC benchmark::benchmark
  PRIVATE myproj::core myproj_warnings myproj_options
)
