# Module and app helpers: one call in libs/<n>/CMakeLists.txt or apps/<n>/CMakeLists.txt.
# Modules register their names so project_check_unit_test_dirs can catch orphaned test directories.

include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/ProjectPackage.cmake)

# Adds every direct subdirectory of the current source directory that has a CMakeLists.txt.
function(project_add_subdirectories)
  file(GLOB children LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
  foreach(child IN LISTS children)
    if(IS_DIRECTORY "${child}" AND EXISTS "${child}/CMakeLists.txt")
      add_subdirectory("${child}")
    endif()
  endforeach()
endfunction()

# project_add_module(NAME <n> [PUBLIC_DEPS ...] [PRIVATE_DEPS ...] [SOURCES ...]
#                    [NO_EXCEPTIONS] [NO_RTTI] [INTERNAL])
# Creates myproj_<n> and myproj::<n>, installed as MyProj::<n> unless INTERNAL.
# No sources under src/ makes it an INTERFACE library.
function(project_add_module)
  cmake_parse_arguments(
    PARSE_ARGV 0 arg "NO_EXCEPTIONS;NO_RTTI;INTERNAL" "NAME" "PUBLIC_DEPS;PRIVATE_DEPS;SOURCES"
  )
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "project_add_module: unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_NAME MATCHES "^[a-z][a-z0-9_]*$")
    message(FATAL_ERROR "project_add_module: NAME must be lower snake_case, got '${arg_NAME}'")
  endif()

  set(target myproj_${arg_NAME})
  set(include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include")

  if(arg_SOURCES)
    set(sources ${arg_SOURCES})
  else()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
  endif()

  if(sources)
    add_library(${target} ${sources})
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${include_dir}>)
    target_compile_features(${target} PUBLIC cxx_std_23)
    # BUILD_INTERFACE keeps the policy targets out of any export set.
    target_link_libraries(
      ${target}
      PUBLIC ${arg_PUBLIC_DEPS}
      PRIVATE
        ${arg_PRIVATE_DEPS}
        $<BUILD_INTERFACE:myproj_warnings>
        $<BUILD_INTERFACE:myproj_options>
    )
    target_compile_options(
      ${target}
      PRIVATE $<$<BOOL:${arg_NO_EXCEPTIONS}>:-fno-exceptions> $<$<BOOL:${arg_NO_RTTI}>:-fno-rtti>
    )
  else()
    if(arg_PRIVATE_DEPS)
      message(FATAL_ERROR "project_add_module(${arg_NAME}): PRIVATE_DEPS needs sources under src/")
    endif()
    add_library(${target} INTERFACE)
    target_include_directories(${target} INTERFACE $<BUILD_INTERFACE:${include_dir}>)
    target_compile_features(${target} INTERFACE cxx_std_23)
    target_link_libraries(${target} INTERFACE ${arg_PUBLIC_DEPS})
  endif()

  add_library(myproj::${arg_NAME} ALIAS ${target})
  set_target_properties(${target} PROPERTIES MYPROJ_INTERNAL "${arg_INTERNAL}")
  set_property(GLOBAL APPEND PROPERTY MYPROJ_MODULES ${arg_NAME})
  if(PROJECT_INSTALL AND NOT arg_INTERNAL)
    _project_install_module(${arg_NAME} ${target} "${include_dir}")
  endif()

  set(test_dir "${PROJECT_SOURCE_DIR}/tests/unit/${arg_NAME}")
  if(PROJECT_BUILD_TESTS AND IS_DIRECTORY "${test_dir}")
    _project_add_unit_tests(${arg_NAME} "${test_dir}")
  endif()
endfunction()

function(_project_add_unit_tests name test_dir)
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${test_dir}/*.cpp")
  _project_add_gtest(${name}_unit_tests SOURCES ${sources} DEPS myproj::${name})
  # A directory label, because CMake 3.28 keeps only the first entry of a LABELS list
  # passed through gtest_discover_tests. libs/<n> holds no other tests.
  set_property(DIRECTORY APPEND PROPERTY LABELS unit ${name})
endfunction()

# project_add_tests(KIND <kind> [DEPS ...])
# Creates <kind>_tests from the .cpp files under the current directory, labeled <kind>.
function(project_add_tests)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "KIND" "DEPS")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "project_add_tests: unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_KIND MATCHES "^[a-z]+$")
    message(FATAL_ERROR "project_add_tests: KIND must be lower case, got '${arg_KIND}'")
  endif()
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
  _project_add_gtest(${arg_KIND}_tests SOURCES ${sources} DEPS ${arg_DEPS})
  set_property(DIRECTORY APPEND PROPERTY LABELS ${arg_KIND})
endfunction()

# Builds a GoogleTest executable on the test support library and registers its tests.
# Suites whose name ends in Stress, typed suites included, are also labeled stress,
# which only the stress preset runs. A typed suite lists as Suite/<n>.Test, so the second
# pattern needs the dot: a parameterized Suite.TestStress/<param> is not a stress test.
function(_project_add_gtest target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEPS")
  add_executable(${target} ${arg_SOURCES})
  target_link_libraries(
    ${target}
    PRIVATE ${arg_DEPS} myproj_test_support myproj_warnings myproj_options
  )
  if(ENABLE_COVERAGE)
    enable_coverage(${target})
  endif()
  gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST TEST_FILTER "-*Stress.*:*Stress/*.*")
  gtest_discover_tests(
    ${target}
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "*Stress.*:*Stress/*.*"
    PROPERTIES LABELS stress
  )
endfunction()

# project_add_app(NAME <n> [DEPS ...] [SOURCES ...])
# Creates executable <n> from the .cpp files under apps/<n>/.
function(project_add_app)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "NAME" "DEPS;SOURCES")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "project_add_app: unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_NAME MATCHES "^[a-z][a-z0-9_]*$")
    message(FATAL_ERROR "project_add_app: NAME must be lower snake_case, got '${arg_NAME}'")
  endif()

  if(arg_SOURCES)
    set(sources ${arg_SOURCES})
  else()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
  endif()
  add_executable(${arg_NAME} ${sources})
  target_link_libraries(${arg_NAME} PRIVATE ${arg_DEPS} myproj_warnings myproj_options)
endfunction()

# project_add_benchmark(MODULE <n> [DEPS ...] [SOURCES ...])
# Creates <n>_benchmarks from the .cpp files under benchmarks/<n>/, linked to myproj::<n> and
# the bench support main. The run_benchmarks target runs every one of them.
function(project_add_benchmark)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "MODULE" "DEPS;SOURCES")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "project_add_benchmark: unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT TARGET myproj::${arg_MODULE})
    message(FATAL_ERROR "project_add_benchmark: no module '${arg_MODULE}' under libs/")
  endif()

  if(arg_SOURCES)
    set(sources ${arg_SOURCES})
  else()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
  endif()
  set(target ${arg_MODULE}_benchmarks)
  add_executable(${target} ${sources})
  target_link_libraries(
    ${target}
    PRIVATE myproj::${arg_MODULE} ${arg_DEPS} myproj_bench_support myproj_warnings myproj_options
  )
  set_property(GLOBAL APPEND PROPERTY MYPROJ_BENCHMARKS ${target})
endfunction()

# Adds run_benchmarks, which runs every benchmark one after another with repetitions,
# so the percentiles have samples, and writes <binary dir>/bench/<target>.json.
# Call after every project_add_benchmark.
function(project_add_benchmark_run_target)
  get_property(targets GLOBAL PROPERTY MYPROJ_BENCHMARKS)
  set(out_dir "${PROJECT_BINARY_DIR}/bench")
  set(commands COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}")
  foreach(target IN LISTS targets)
    list(
      APPEND commands
      COMMAND
        $<TARGET_FILE:${target}> --benchmark_repetitions=10 --benchmark_display_aggregates_only=true
        --benchmark_out=${out_dir}/${target}.json --benchmark_out_format=json
    )
  endforeach()
  add_custom_target(
    run_benchmarks
    ${commands}
    DEPENDS ${targets}
    USES_TERMINAL
    VERBATIM
    COMMENT "Writing benchmark results to ${out_dir}"
  )
endfunction()

# project_add_fuzz_target(NAME <n> [DEPS ...] [SOURCES ...])
# Creates the libFuzzer executable <n> from <n>.cpp in the current directory, one per harness.
# CTest replays the seed corpus in corpus/<n>/, labeled fuzz; run_fuzz looks for new inputs.
function(project_add_fuzz_target)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "NAME" "DEPS;SOURCES")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "project_add_fuzz_target: unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT arg_NAME MATCHES "^[a-z][a-z0-9_]*$")
    message(FATAL_ERROR "project_add_fuzz_target: NAME must be lower snake_case, got '${arg_NAME}'")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzz targets need libFuzzer, which only Clang ships; use the fuzz preset.")
  endif()
  set(seeds "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${arg_NAME}")
  if(NOT IS_DIRECTORY "${seeds}")
    message(FATAL_ERROR "project_add_fuzz_target(${arg_NAME}): add at least one seed input to ${seeds}")
  endif()

  if(arg_SOURCES)
    set(sources ${arg_SOURCES})
  else()
    set(sources "${CMAKE_CURRENT_SOURCE_DIR}/${arg_NAME}.cpp")
  endif()
  add_executable(${arg_NAME} ${sources})
  target_link_libraries(${arg_NAME} PRIVATE ${arg_DEPS} myproj_warnings myproj_options)
  # PROJECT_BUILD_FUZZ gives every internal target fuzzer-no-link; only a harness links the driver.
  target_compile_options(${arg_NAME} PRIVATE -fsanitize=fuzzer)
  target_link_options(${arg_NAME} PRIVATE -fsanitize=fuzzer)
  if(APPLE)
    # Apple ld rejects some harness objects that combine this check with fuzzer coverage:
    # "invalid r_symbolnum". The modules under test keep the check. A source option follows
    # the -fsanitize flags of myproj_options on the command line, so this one wins.
    set_property(SOURCE ${sources} APPEND PROPERTY COMPILE_OPTIONS -fno-sanitize=function)
  endif()
  set_target_properties(${arg_NAME} PROPERTIES MYPROJ_FUZZ_SEEDS "${seeds}")
  set_property(GLOBAL APPEND PROPERTY MYPROJ_FUZZ_TARGETS ${arg_NAME})

  add_test(NAME ${arg_NAME}.ReplaysTheSeedCorpus COMMAND ${arg_NAME} -runs=0 "${seeds}")
  set_tests_properties(${arg_NAME}.ReplaysTheSeedCorpus PROPERTIES LABELS fuzz)
endfunction()

# Adds run_fuzz, which fuzzes every harness for PROJECT_FUZZ_SECONDS, one after another.
# New inputs and crash files go to <binary dir>/fuzz/<target>/, never to the seed corpus.
# Call after every project_add_fuzz_target.
function(project_add_fuzz_run_target)
  get_property(targets GLOBAL PROPERTY MYPROJ_FUZZ_TARGETS)
  set(commands "")
  foreach(target IN LISTS targets)
    get_target_property(seeds ${target} MYPROJ_FUZZ_SEEDS)
    set(out_dir "${PROJECT_BINARY_DIR}/fuzz/${target}")
    list(
      APPEND commands
      COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}/corpus"
      COMMAND
        $<TARGET_FILE:${target}> -max_total_time=${PROJECT_FUZZ_SECONDS} -print_final_stats=1
        -artifact_prefix=${out_dir}/ "${out_dir}/corpus" "${seeds}"
    )
  endforeach()
  add_custom_target(
    run_fuzz
    ${commands}
    DEPENDS ${targets}
    USES_TERMINAL
    VERBATIM
    COMMENT "Fuzzing each harness for ${PROJECT_FUZZ_SECONDS} s"
  )
endfunction()

# Fails configure when tests/unit/<n> exists without a module <n>. Call after libs/ is added.
function(project_check_unit_test_dirs)
  get_property(modules GLOBAL PROPERTY MYPROJ_MODULES)
  file(GLOB entries LIST_DIRECTORIES true CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/unit/*")
  foreach(entry IN LISTS entries)
    get_filename_component(name "${entry}" NAME)
    if(IS_DIRECTORY "${entry}" AND NOT name IN_LIST modules)
      message(
        FATAL_ERROR
          "tests/unit/${name} has no matching module. Create libs/${name} with "
          "project_add_module(NAME ${name}), or rename or remove tests/unit/${name}."
      )
    endif()
  endforeach()
endfunction()
