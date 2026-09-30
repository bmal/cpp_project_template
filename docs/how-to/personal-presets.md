# Keep a personal preset

Every configure points `build/current` and the root `compile_commands.json` at its build directory,
so clangd, debuggers, and scripts follow the last configured preset.

## Add a preset of your own

When you want a shared preset with one setting changed, only on your machine,
write `CMakeUserPresets.json` at the repository root; git ignores it:

```json
{
  "version": 8,
  "configurePresets": [{ "name": "dev-mine", "inherits": "dev",
    "cacheVariables": { "PROJECT_LINKER": "default" } }]
}
```

When you want to build it, configure it and build whatever `build/current` points at:

```bash
cmake --preset dev-mine
cmake --build build/current
```
