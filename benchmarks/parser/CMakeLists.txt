# Mechanism: a benchmark executable registered in one call, named after the module it measures.
# Every .cpp here becomes parser_benchmarks, linked to myproj::parser and the bench support main.
project_add_benchmark(MODULE parser)
