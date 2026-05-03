cmake_minimum_required(VERSION 3.20...3.30)

include(../cmake/ProjectIdentity.cmake)

project(${TEMPLATE_STANDALONE_PROJECT} LANGUAGES CXX)

# --- Import tools ----
include(../cmake/tools.cmake)

# ---- Dependencies ----
include(../cmake/CPM.cmake)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

if(NOT TARGET ${TEMPLATE_NAMESPACED_TARGET})
  CPMAddPackage(
    NAME ${TEMPLATE_PROJECT_NAME}
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..
  )
endif()

# ---- Create standalone executable ----
file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${TEMPLATE_STANDALONE_TARGET} ${sources})
target_link_libraries(${TEMPLATE_STANDALONE_TARGET} PRIVATE ${TEMPLATE_NAMESPACED_TARGET})

target_compile_features(${TEMPLATE_STANDALONE_TARGET} PRIVATE cxx_std_23)
set_target_properties(
  ${TEMPLATE_STANDALONE_TARGET}
  PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(
  TARGET ${TEMPLATE_STANDALONE_TARGET}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TEMPLATE_STANDALONE_TARGET}>
          ${CMAKE_BINARY_DIR}/bin/standalone
  COMMENT "Updating stable standalone launcher"
)

# ---- Enable warnings ----
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${TEMPLATE_STANDALONE_TARGET} PRIVATE -Wall -Wpedantic -Wextra -Werror)
elseif(MSVC)
    target_compile_options(${TEMPLATE_STANDALONE_TARGET} PRIVATE /W4 /WX)
endif()

add_custom_target(
  run-standalone
  COMMAND $<TARGET_FILE:${TEMPLATE_STANDALONE_TARGET}>
  DEPENDS ${TEMPLATE_STANDALONE_TARGET}
  COMMENT "Running ${TEMPLATE_STANDALONE_TARGET}"
)
