# Mechanism: a compiled module registered in one call, plus the configure-time version header.
# Sources under src/ make myproj_core a real library; project_add_version_header adds myproj/version.hpp.
project_add_module(NAME core PRIVATE_DEPS fmt::fmt myproj::parser)
project_add_version_header(core)
