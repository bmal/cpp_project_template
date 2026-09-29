# Sanitizer policy: PROJECT_SANITIZER instruments every internal target through myproj_options.
# Dependencies get the same instrumentation from the matching triplet, which the preset selects.

set(
  PROJECT_SANITIZER
  ""
  CACHE STRING
  "Sanitizers for every internal target: a semicolon list of address, undefined, thread, memory, leak"
)
set_property(
  CACHE PROJECT_SANITIZER
  PROPERTY STRINGS address undefined thread memory leak
)

set(
  _project_known_sanitizers
  address
  undefined
  thread
  memory
  leak
)
foreach(_project_sanitizer IN LISTS PROJECT_SANITIZER)
  if(NOT _project_sanitizer IN_LIST _project_known_sanitizers)
    message(
      FATAL_ERROR
      "PROJECT_SANITIZER accepts address, undefined, thread, memory, and leak, separated by semicolons, not '${_project_sanitizer}'."
    )
  endif()
endforeach()

# Address, thread, and memory each own the shadow memory, and leak works alone or with address.
set(_project_exclusive "")
foreach(_project_sanitizer IN ITEMS address thread memory)
  if(_project_sanitizer IN_LIST PROJECT_SANITIZER)
    list(APPEND _project_exclusive ${_project_sanitizer})
  endif()
endforeach()
if("leak" IN_LIST PROJECT_SANITIZER AND NOT "address" IN_LIST PROJECT_SANITIZER)
  list(APPEND _project_exclusive leak)
endif()
list(LENGTH _project_exclusive _project_exclusive_count)
if(_project_exclusive_count GREATER 1)
  message(
    FATAL_ERROR
    "PROJECT_SANITIZER cannot combine ${_project_exclusive} in one build; pick one."
  )
endif()
if("memory" IN_LIST PROJECT_SANITIZER AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "PROJECT_SANITIZER=memory needs Clang; GCC has no MemorySanitizer.")
endif()

# Without llvm-symbolizer a Clang report on Linux has no function names, and no suppression matches.
if(
  (PROJECT_SANITIZER OR PROJECT_BUILD_FUZZ)
  AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"
  AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
)
  cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _project_compiler_bin)
  string(REGEX MATCH "^[0-9]+" _project_clang_major "${CMAKE_CXX_COMPILER_VERSION}")
  find_program(
    _project_symbolizer
    NAMES "llvm-symbolizer-${_project_clang_major}" llvm-symbolizer
    HINTS "${_project_compiler_bin}" NO_CACHE
  )
  if(NOT _project_symbolizer)
    message(
      FATAL_ERROR
      "Sanitizer reports need llvm-symbolizer to name functions, and it was not found; install it with:\n  scripts/bootstrap.sh\n"
    )
  endif()
endif()

set(_project_sanitize_flags ${PROJECT_SANITIZER})
# Fuzz harnesses need coverage instrumentation in every module they call; only a harness links the driver.
if(PROJECT_BUILD_FUZZ)
  list(APPEND _project_sanitize_flags fuzzer-no-link)
endif()

if(_project_sanitize_flags)
  list(JOIN _project_sanitize_flags "," _project_sanitize)
  target_compile_options(
    myproj_options
    INTERFACE -fsanitize=${_project_sanitize} -fno-sanitize-recover=all -fno-omit-frame-pointer
  )
  target_link_options(myproj_options INTERFACE -fsanitize=${_project_sanitize})
endif()
# A MemorySanitizer report then names where the uninitialized value was allocated, not only where it was read.
if("memory" IN_LIST PROJECT_SANITIZER)
  target_compile_options(myproj_options INTERFACE -fsanitize-memory-track-origins=2)
endif()
