# Mechanism: an app is a directory under apps/ with one project_add_app call naming its modules.
# The executable lands in build/<preset>/bin/myproj_cli.
project_add_app(NAME myproj_cli DEPS myproj::core myproj::parser)
