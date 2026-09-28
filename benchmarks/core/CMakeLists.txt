# Interim wiring until project_add_benchmark lands (#13).
file(GLOB sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(core_benchmarks ${sources})
target_link_libraries(
  core_benchmarks
  PRIVATE myproj::core benchmark::benchmark myproj_warnings myproj_options
)
