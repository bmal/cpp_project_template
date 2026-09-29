# Mechanism: a compiled module registered in one call, plus the configure-time version header.
# Sources under src/ make myproj_core a real library; project_add_version_header adds myproj/version.hpp.
project_add_module(NAME core PUBLIC_DEPS fmt::fmt)
project_add_version_header(core)
