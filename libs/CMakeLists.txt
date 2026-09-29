# Mechanism: modules self-register; every subdirectory with a CMakeLists.txt is added as a module.
# Adding a module never edits this file; see project_add_subdirectories in cmake/ProjectModules.cmake.
project_add_subdirectories()
