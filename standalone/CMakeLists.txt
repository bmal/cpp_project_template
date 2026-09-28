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
target_compile_options(${TEMPLATE_STANDALONE_TARGET} PRIVATE -Wall -Wpedantic -Wextra -Werror)

add_custom_target(
  run-standalone
  COMMAND $<TARGET_FILE:${TEMPLATE_STANDALONE_TARGET}>
  DEPENDS ${TEMPLATE_STANDALONE_TARGET}
  COMMENT "Running ${TEMPLATE_STANDALONE_TARGET}"
)
