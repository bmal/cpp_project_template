# Lint target: clang-tidy over this build's compile_commands.json, with the checks in .clang-tidy.
# It prefers the clang-tidy that matches the project's Clang, and fails only when run without one.

function(project_add_lint_target)
  # clang++-22 makes run-clang-tidy-22 the first choice; otherwise the newest version wins.
  cmake_path(GET CMAKE_CXX_COMPILER FILENAME compiler_name)
  cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_bin)
  set(names "")
  if(compiler_name MATCHES "^clang\\+\\+(-[0-9]+)?$")
    list(APPEND names "run-clang-tidy${CMAKE_MATCH_1}")
  endif()
  foreach(version RANGE 30 19 -1)
    list(APPEND names "run-clang-tidy-${version}")
  endforeach()
  list(APPEND names run-clang-tidy)
  # Name order decides, and nothing is cached, so a changed compiler picks its own clang-tidy.
  find_program(
    run_clang_tidy
    NAMES ${names}
    HINTS "${compiler_bin}" /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
    NO_CACHE
  )

  if(NOT run_clang_tidy)
    add_custom_target(
      lint
      COMMAND "${CMAKE_COMMAND}" -E echo "lint: run-clang-tidy was not found; run scripts/bootstrap.sh"
      COMMAND "${CMAKE_COMMAND}" -E false
      VERBATIM
    )
    return()
  endif()

  # run-clang-tidy-22 sits next to clang-tidy-22.
  cmake_path(GET run_clang_tidy PARENT_PATH tidy_bin)
  cmake_path(GET run_clang_tidy FILENAME runner_name)
  string(REPLACE "run-clang-tidy" "clang-tidy" tidy_name "${runner_name}")
  add_custom_target(
    lint
    COMMAND
      "${run_clang_tidy}" -p "${PROJECT_BINARY_DIR}" -clang-tidy-binary "${tidy_bin}/${tidy_name}" -quiet
      # GCC-only warning flags in a GCC build are unknown to clang-tidy.
      -extra-arg=-Wno-unknown-warning-option
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()
