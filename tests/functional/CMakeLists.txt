# Mechanism: black-box tests that spawn a built app, labeled functional and run by their own preset.
# Building functional_tests builds the app, and the tests find it at the path compiled in here.
project_add_tests(KIND functional)
add_dependencies(functional_tests myproj_cli)
target_compile_definitions(functional_tests PRIVATE MYPROJ_CLI_PATH="$<TARGET_FILE:myproj_cli>")
