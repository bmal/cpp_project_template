# build/current and the root compile_commands.json follow the last configured build directory,
# so clangd, debuggers, and scripts use one fixed path whatever the preset.

function(project_link_current_build)
  set(current "${PROJECT_SOURCE_DIR}/build/current")
  if(PROJECT_BINARY_DIR STREQUAL current)
    return()
  endif()
  if(IS_DIRECTORY "${current}" AND NOT IS_SYMLINK "${current}")
    message(
      WARNING
      "build/current is a directory, not a link; delete it to let configure refresh it."
    )
    return()
  endif()
  file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/build")
  file(CREATE_LINK "${PROJECT_BINARY_DIR}" "${current}" SYMBOLIC)
  # Relative through build/current, so this link never needs to change.
  file(
    CREATE_LINK "build/current/compile_commands.json" "${PROJECT_SOURCE_DIR}/compile_commands.json"
    SYMBOLIC
  )
endfunction()
