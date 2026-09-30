# Replay a failure with rr

rr records one run of a program on Linux and replays it under gdb, backwards as well as forwards.
It needs the CPU's hardware performance counters; the dev container on a Mac and most cloud virtual machines have none.

## Install rr

When you first use rr on a machine, install it and gdb:

```bash
sudo apt-get install rr gdb
```

Then let it read the performance counters, until the next reboot:

```bash
sudo sysctl kernel.perf_event_paranoid=1
```

## Record a failing test

When a test fails and you want to see how it got there, record one run of it:

```bash
rr record build/current/bin/core_unit_tests --gtest_filter='Counter.*'
```

You should see the test's own output; the recording goes under `~/.local/share/rr/`.

## Replay it backwards

When you want to walk back from the failure, replay the latest recording:

```bash
rr replay
```

You should see a gdb prompt. `continue` runs to the end; `reverse-continue`, `reverse-next`, and `watch -l <expression>` go back from there.
Every replay runs the same instructions, so addresses and values repeat.
