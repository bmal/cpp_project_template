function(enable_coverage target_name)
  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "Coverage target '${target_name}' does not exist")
  endif()

  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Code coverage is most useful with a Debug build")
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang")
    message(STATUS "Enabling LLVM coverage for ${target_name}")
    target_compile_options(${target_name} PRIVATE -fprofile-instr-generate -fcoverage-mapping -O0 -g)
    target_link_options(${target_name} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Enabling GCC/lcov coverage for ${target_name}")
    target_compile_options(${target_name} PRIVATE --coverage -O0 -g)
    target_link_options(${target_name} PRIVATE --coverage)

    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)

    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
      set(COVERAGE_INFO "${CMAKE_BINARY_DIR}/coverage.info")
      set(COVERAGE_REPORT_DIR "${CMAKE_BINARY_DIR}/coverage_report")

      add_custom_target(
        ${target_name}_coverage_report
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_REPORT_DIR}
        COMMAND ${LCOV_EXECUTABLE} --capture --initial --directory . --output-file
                ${COVERAGE_INFO}.base
        COMMAND ${LCOV_EXECUTABLE} --capture --directory . --output-file ${COVERAGE_INFO}.test
        COMMAND ${LCOV_EXECUTABLE} --add-tracefile ${COVERAGE_INFO}.base --add-tracefile
                ${COVERAGE_INFO}.test --output-file ${COVERAGE_INFO}.total
        COMMAND
          ${LCOV_EXECUTABLE} --remove ${COVERAGE_INFO}.total '${CMAKE_BINARY_DIR}/*'
          '${CMAKE_SOURCE_DIR}/test/*' '${CMAKE_SOURCE_DIR}/build/*' '${CMAKE_SOURCE_DIR}/_deps/*'
          '/usr/include/*' '/usr/lib/*' --output-file ${COVERAGE_INFO}
        COMMAND ${GENHTML_EXECUTABLE} --demangle-cpp -o ${COVERAGE_REPORT_DIR} ${COVERAGE_INFO}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report for ${target_name}"
      )
    else()
      message(STATUS "lcov/genhtml not found; coverage flags are enabled but no report target was added")
    endif()
  else()
    message(WARNING "Coverage is not configured for compiler ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()
