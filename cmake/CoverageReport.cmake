# Script behind the coverage target: runs every test but stress on fresh counters, then writes
# BINARY_DIR/coverage/lcov.info with records for SOURCE_DIR/libs/ only.
# Clang: llvm_profdata and llvm_cov. GCC: gcov and lcov. Both: COMPILER_ID, SOURCE_DIR, BINARY_DIR, CTEST.

set(out "${BINARY_DIR}/coverage")
file(REMOVE_RECURSE "${out}")
file(MAKE_DIRECTORY "${out}")
file(GLOB_RECURSE old_counters "${BINARY_DIR}/*.gcda")
if(old_counters)
  file(REMOVE ${old_counters})
endif()

# %p keeps processes apart, and %m keeps one file per binary when a process runs several times.
set(ENV{LLVM_PROFILE_FILE} "${out}/profraw/%p-%m.profraw")
execute_process(
  COMMAND "${CTEST}" --test-dir "${BINARY_DIR}" --label-exclude "^stress$" --output-on-failure
  COMMAND_ERROR_IS_FATAL ANY
)

if(COMPILER_ID MATCHES "Clang")
  file(GLOB profiles "${out}/profraw/*.profraw")
  execute_process(
    COMMAND "${llvm_profdata}" merge -sparse ${profiles} -o "${out}/coverage.profdata"
    COMMAND_ERROR_IS_FATAL ANY
  )
  file(STRINGS "${BINARY_DIR}/coverage-objects.txt" objects)
  if(NOT objects)
    message(FATAL_ERROR "coverage: this build has no test executable to report on.")
  endif()
  list(POP_FRONT objects first)
  list(TRANSFORM objects PREPEND "-object=")
  execute_process(
    COMMAND
      "${llvm_cov}" export -format=lcov "-instr-profile=${out}/coverage.profdata" "${first}"
      ${objects} "${SOURCE_DIR}/libs"
    OUTPUT_FILE "${out}/lcov.info"
    COMMAND_ERROR_IS_FATAL ANY
  )
else()
  # GCC 14 reports end lines of GoogleTest bodies that lcov 2 calls a mismatch; the lines still count.
  execute_process(
    COMMAND
      "${lcov}" --quiet --capture --directory "${BINARY_DIR}" --gcov-tool "${gcov}" --ignore-errors
      mismatch --output-file "${out}/all.info"
    COMMAND_ERROR_IS_FATAL ANY
  )
  execute_process(
    COMMAND
      "${lcov}" --quiet --extract "${out}/all.info" "${SOURCE_DIR}/libs/*" --output-file
      "${out}/lcov.info"
    COMMAND_ERROR_IS_FATAL ANY
  )
endif()
message(STATUS "Coverage: ${out}/lcov.info")
