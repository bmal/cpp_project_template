# Build tools of the top-level build: ccache, the fastest linker that works, and a checked LTO.
# A consumer that adds this project as a subdirectory keeps its own launcher, linker, and LTO choice.

include(CheckIPOSupported)
include(CheckLinkerFlag)

set(_project_linkers auto mold lld default)
set(PROJECT_LINKER auto CACHE STRING "Linker: auto, mold, lld, or default (the compiler's own)")
set_property(CACHE PROJECT_LINKER PROPERTY STRINGS ${_project_linkers})

# Succeeds when this compiler links with -fuse-ld=$1, with LTO too when the build asks for it.
function(_project_linker_works linker result)
  string(MAKE_C_IDENTIFIER "_project_linker_${linker}_works" flag_var)
  check_linker_flag(CXX "-fuse-ld=${linker}" ${flag_var})
  set(works ${${flag_var}})
  if(NOT works)
    # Forget a miss, so a linker installed later is found by the next configure.
    unset(${flag_var} CACHE)
  endif()
  if(works AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    # The LTO check links with CMAKE_CXX_FLAGS, so the linker under test goes there.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=${linker}")
    check_ipo_supported(RESULT works OUTPUT ignored LANGUAGES CXX)
  endif()
  set(${result} ${works} PARENT_SCOPE)
endfunction()

function(project_configure_build_tools)
  if(NOT DEFINED CACHE{CMAKE_CXX_COMPILER_LAUNCHER})
    find_program(_project_ccache ccache)
    mark_as_advanced(_project_ccache)
    if(_project_ccache)
      set(
        CMAKE_CXX_COMPILER_LAUNCHER
        "${_project_ccache}"
        CACHE STRING
        "Compiler launcher; set it empty to build without ccache"
      )
    endif()
  endif()

  if(NOT PROJECT_LINKER IN_LIST _project_linkers)
    message(
      FATAL_ERROR
      "PROJECT_LINKER must be auto, mold, lld, or default, not '${PROJECT_LINKER}'."
    )
  endif()
  set(linker "")
  if(PROJECT_LINKER STREQUAL "auto")
    # macOS keeps the Apple linker. lld cannot link GCC's LTO objects, so the check skips it there.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      foreach(candidate IN ITEMS mold lld)
        _project_linker_works(${candidate} works)
        if(works)
          set(linker ${candidate})
          break()
        endif()
      endforeach()
    endif()
  elseif(NOT PROJECT_LINKER STREQUAL "default")
    _project_linker_works(${PROJECT_LINKER} works)
    if(NOT works)
      message(
        FATAL_ERROR
        "PROJECT_LINKER=${PROJECT_LINKER} cannot link this build; install it or configure with -DPROJECT_LINKER=auto."
      )
    endif()
    set(linker ${PROJECT_LINKER})
  endif()
  if(linker)
    add_link_options(-fuse-ld=${linker})
    # Local to this function: the LTO check below links with the chosen linker.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=${linker}")
    message(STATUS "Linker: ${linker}")
  else()
    message(STATUS "Linker: the compiler's default")
  endif()

  if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    check_ipo_supported(RESULT lto_works OUTPUT lto_error LANGUAGES CXX)
    if(NOT lto_works)
      message(
        FATAL_ERROR
        "LTO does not work with this compiler and linker; configure with -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF.\n${lto_error}"
      )
    endif()
  endif()
endfunction()
