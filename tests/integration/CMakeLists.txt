# Mechanism: one integration executable over several modules, labeled integration by the helper.
# Suites named *Stress are labeled stress too, so the dev preset leaves them out.
project_add_tests(KIND integration DEPS myproj::core)
