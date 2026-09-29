# Format targets: format rewrites and format-check verifies every C++ and CMake file.
# Both call scripts/format.sh, the script make format and the pre-commit hooks call.

function(project_add_format_targets)
  add_custom_target(
    format
    COMMAND "${PROJECT_SOURCE_DIR}/scripts/format.sh"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
  )
  add_custom_target(
    format-check
    COMMAND "${PROJECT_SOURCE_DIR}/scripts/format.sh" --check
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()
