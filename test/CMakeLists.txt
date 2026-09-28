option(ENABLE_COVERAGE "Enable test coverage" OFF)

include(../cmake/Coverage.cmake)

find_package(GTest CONFIG REQUIRED)

add_subdirectory(functional)

if(PROJECT_BUILD_BENCHMARKS)
  find_package(benchmark CONFIG REQUIRED)
  add_subdirectory(benchmark)
endif()
