# Install, version header, and packaging. Non-INTERNAL modules join the MyProjTargets export set
# through project_add_module; project_install_package writes the package files and sets up CPack.

include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PROJECT_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# Called by project_add_module: installs the module and its public headers as MyProj::<name>.
function(_project_install_module name target include_dir)
  set_target_properties(${target} PROPERTIES EXPORT_NAME ${name})
  install(
    TARGETS ${target}
    EXPORT ${PROJECT_NAME}Targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  if(IS_DIRECTORY "${include_dir}")
    install(DIRECTORY "${include_dir}/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  endif()
endfunction()

# project_add_version_header(<module>)
# Generates <build>/generated/include/myproj/version.hpp from project(VERSION) and the git commit
# at configure time, and makes it part of myproj::<module> and of its installed headers.
function(project_add_version_header module)
  find_package(Git QUIET)
  set(PROJECT_GIT_COMMIT "unknown")
  set(PROJECT_GIT_DIRTY "false")
  if(GIT_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_VARIABLE commit
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE commit_result
      ERROR_QUIET
    )
    if(commit_result EQUAL 0)
      set(PROJECT_GIT_COMMIT "${commit}")
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE changes
        ERROR_QUIET
      )
      if(changes)
        set(PROJECT_GIT_DIRTY "true")
      endif()
    endif()
  endif()

  # A version without a patch or minor part still yields valid integers.
  foreach(part MAJOR MINOR PATCH)
    if(NOT PROJECT_VERSION_${part})
      set(PROJECT_VERSION_${part} 0)
    endif()
  endforeach()

  set(generated_dir "${PROJECT_BINARY_DIR}/generated/include")
  configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/templates/version.hpp.in"
    "${generated_dir}/myproj/version.hpp"
    @ONLY
  )

  set(target myproj_${module})
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "INTERFACE_LIBRARY")
    target_include_directories(${target} INTERFACE $<BUILD_INTERFACE:${generated_dir}>)
  else()
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${generated_dir}>)
  endif()
  get_target_property(internal ${target} MYPROJ_INTERNAL)
  if(NOT internal)
    install(DIRECTORY "${generated_dir}/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  endif()
endfunction()

# Writes the package config, version, and targets files, and configures CPack. Call last.
function(project_install_package)
  # An installed module cannot reach a module that is not installed.
  get_property(modules GLOBAL PROPERTY MYPROJ_MODULES)
  foreach(module IN LISTS modules)
    get_target_property(internal myproj_${module} MYPROJ_INTERNAL)
    if(internal)
      continue()
    endif()
    get_target_property(deps myproj_${module} INTERFACE_LINK_LIBRARIES)
    get_target_property(private_deps myproj_${module} LINK_LIBRARIES)
    foreach(dep IN LISTS deps private_deps)
      if(dep MATCHES "^myproj(::|_)([a-z0-9_]+)$" AND CMAKE_MATCH_2 IN_LIST modules)
        get_target_property(dep_internal myproj_${CMAKE_MATCH_2} MYPROJ_INTERNAL)
        if(dep_internal)
          message(
            FATAL_ERROR
              "Module ${module} is installed but links INTERNAL module ${CMAKE_MATCH_2}. "
              "Remove INTERNAL from ${CMAKE_MATCH_2}, or add INTERNAL to ${module}."
          )
        endif()
      endif()
    endforeach()
  endforeach()

  install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${PROJECT_INSTALL_CMAKEDIR}
  )
  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/templates/Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${PROJECT_INSTALL_CMAKEDIR}
  )
  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
  )
  install(
    FILES
      "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
      "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${PROJECT_INSTALL_CMAKEDIR}
  )

  set(CPACK_GENERATOR "TGZ" PARENT_SCOPE)
  set(CPACK_SOURCE_GENERATOR "TGZ" PARENT_SCOPE)
  set(CPACK_SOURCE_IGNORE_FILES "/[.]git/" "/build/" "/[.]vcpkg/" "/vcpkg_installed/" PARENT_SCOPE)
  set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/package" PARENT_SCOPE)
endfunction()
