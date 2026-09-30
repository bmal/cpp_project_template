# Debug a test

Open the folder in VS Code, install the recommended extensions, and pick a configure preset in the CMake Tools status bar.
The Testing view lists every `build/current/bin/*_tests`, so it follows that preset.

## Stop in one test

When you want one test under the debugger, set a breakpoint in it and choose **Debug Test** on it in the Testing view.

## Stop in the tests a filter selects

When you want a GoogleTest filter, run **CMake: Set Launch/Debug Target**, pick the test executable, start **Debug tests by filter** from the Run view, and enter the filter:

```text
Counter.AddAccumulates
```

## Debug from a terminal

When you are not in VS Code, pass the filter after `--`:

```bash
lldb build/current/bin/core_unit_tests -- --gtest_filter='Counter.*'
```

## Stop at a sanitizer report in lldb or gdb

When you want terminal lldb or gdb to stop after a sanitizer report, with the failing frames live, load the project's settings once per machine:

```bash
echo "command source $PWD/tools/lldbinit" >> ~/.lldbinit
```

```bash
echo "source $PWD/tools/gdbinit" >> ~/.gdbinit
```

VS Code needs neither: its launches and **Debug Test** set `abort_on_error=1`, so a report stops the debugger.
