# Coverage: ENABLE_COVERAGE instruments every internal target through myproj_options, and the
# coverage target writes coverage/lcov.info, from llvm-cov on Clang and from gcov and lcov on GCC.

function(project_configure_coverage)
  if(NOT ENABLE_COVERAGE)
    return()
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(flags -fprofile-instr-generate -fcoverage-mapping)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(flags --coverage)
  else()
    message(FATAL_ERROR "ENABLE_COVERAGE needs Clang or GCC, not ${CMAKE_CXX_COMPILER_ID}.")
  endif()
  target_compile_options(myproj_options INTERFACE ${flags})
  target_link_options(myproj_options INTERFACE ${flags})
endfunction()

# Call after every test target exists: the coverage target builds them, runs them, and reports.
function(project_add_coverage_target)
  if(NOT ENABLE_COVERAGE OR NOT PROJECT_BUILD_TESTS)
    return()
  endif()
  cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_bin)
  string(REGEX MATCH "^[0-9]+" major "${CMAKE_CXX_COMPILER_VERSION}")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(tools llvm-profdata llvm-cov)
  else()
    set(tools gcov lcov)
  endif()
  set(tool_args "")
  foreach(tool IN LISTS tools)
    string(REPLACE "-" "_" var "${tool}")
    # The versioned name first, so the tool reads the format this compiler writes.
    find_program(${var} NAMES "${tool}-${major}" "${tool}" HINTS "${compiler_bin}" NO_CACHE)
    if(NOT ${var})
      add_custom_target(
        coverage
        COMMAND
          "${CMAKE_COMMAND}" -E echo "coverage: ${tool} was not found; run scripts/bootstrap.sh"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
      )
      return()
    endif()
    list(APPEND tool_args "-D${var}=${${var}}")
  endforeach()

  get_property(test_targets GLOBAL PROPERTY PROJECT_TEST_TARGETS)
  list(TRANSFORM test_targets REPLACE "^.+$" "$<TARGET_FILE:\\0>" OUTPUT_VARIABLE test_files)
  list(JOIN test_files "\n" test_files)
  file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/coverage-objects.txt" CONTENT "${test_files}\n")

  add_custom_target(
    coverage
    COMMAND
      "${CMAKE_COMMAND}" ${tool_args} -DCOMPILER_ID=${CMAKE_CXX_COMPILER_ID}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}"
      "-DCTEST=${CMAKE_CTEST_COMMAND}" -P "${PROJECT_SOURCE_DIR}/cmake/CoverageReport.cmake"
    COMMENT "Running the tests and writing coverage/lcov.info"
    USES_TERMINAL
    VERBATIM
  )
  add_dependencies(coverage ${test_targets})
endfunction()
