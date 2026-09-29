# Mechanism: apps self-register; every subdirectory with a CMakeLists.txt is added as an app.
# Adding an app never edits this file; see project_add_subdirectories in cmake/ProjectModules.cmake.
project_add_subdirectories()
