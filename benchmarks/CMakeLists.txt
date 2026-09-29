# Benchmarks self-register: every benchmarks/<module>/ with a CMakeLists.txt is added.
# support/ holds the shared main and helpers that every benchmark executable links.
project_add_subdirectories()
project_add_benchmark_run_target()
