# Mechanism: one libFuzzer executable per harness, built only by the fuzz preset.
# Each call names a harness <n>.cpp here and its seed inputs under corpus/<n>/.
project_add_fuzz_target(NAME parser_fuzz DEPS myproj::parser)
