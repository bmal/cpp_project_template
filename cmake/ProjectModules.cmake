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
  set(target ${name}_unit_tests)
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${test_dir}/*.cpp")
  add_executable(${target} ${sources})
  target_link_libraries(
    ${target}
    PRIVATE myproj::${name} GTest::gmock_main myproj_warnings myproj_options
  )
  if(ENABLE_COVERAGE)
    enable_coverage(${target})
  endif()
  gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
  # A directory label, because CMake 3.28 keeps only the first entry of a LABELS list
  # passed through gtest_discover_tests. libs/<n> holds no other tests.
  set_property(DIRECTORY APPEND PROPERTY LABELS unit ${name})
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
