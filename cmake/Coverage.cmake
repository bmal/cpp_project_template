function(enable_coverage project_name)
    # Warn if the build type is not Debug since coverage results may be inaccurate
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Code coverage results with an optimized (non-Debug) build may be misleading")
    endif()

    # Configure coverage flags based on the compiler
    if(CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang" OR CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang")
        message(STATUS "Building with llvm Code Coverage Tools")

        set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")
        set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")
        set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")
        set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")

    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        message(STATUS "Building with lcov Code Coverage Tools")

        set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")
    endif()

    # Set the Coverage build type
    if(ENABLE_COVERAGE)
        set(CMAKE_BUILD_TYPE Coverage CACHE STRING
            "Choose the type of build: None Debug Release RelWithDebInfo MinSizeRel Coverage"
            FORCE)

        # Define locations for coverage files
        set(COVERAGE_INFO "${CMAKE_BINARY_DIR}/coverage.info")
        set(COVERAGE_REPORT_DIR "${CMAKE_BINARY_DIR}/coverage_report")

        # Create custom target for generating coverage report
        add_custom_target(${project_name}_coverage_report
            COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_REPORT_DIR}
            COMMAND lcov --capture --initial --directory . --output-file ${COVERAGE_INFO}.base
            COMMAND lcov --capture --directory . --output-file ${COVERAGE_INFO}.test
            COMMAND lcov
                --add-tracefile ${COVERAGE_INFO}.base
                --add-tracefile ${COVERAGE_INFO}.test
                --output-file ${COVERAGE_INFO}.total
            COMMAND lcov
                --remove ${COVERAGE_INFO}.total
                '${CMAKE_BINARY_DIR}/*'
                '${CMAKE_SOURCE_DIR}/test/*'
                '${CMAKE_SOURCE_DIR}/build/*'
                '${CMAKE_SOURCE_DIR}/_deps/*'
                '/usr/include/*'
                '/usr/lib/*'
                --output-file ${COVERAGE_INFO}
            COMMAND genhtml --demangle-cpp -o ${COVERAGE_REPORT_DIR} ${COVERAGE_INFO}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )
    endif()
endfunction()
