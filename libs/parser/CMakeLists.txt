# Mechanism: a header-only module; with no sources under src/ the helper makes an INTERFACE target.
# NO_EXCEPTIONS records the contract; with no sources to compile, scripts/selftest.sh enforces it.
project_add_module(NAME parser NO_EXCEPTIONS)
