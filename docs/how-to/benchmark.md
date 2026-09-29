# Benchmark a change

`make bench` writes `build/bench/bench/<module>_benchmarks.json`, with p50, p90, and p99 over ten repetitions.

When you want numbers you can trust on Linux, prepare the machine in one terminal and leave it running:

```bash
scripts/bench-env.sh
```

It sets the performance governor, turns turbo off, and prints a `taskset` command for a second terminal.
Press Enter in the first terminal to restore the machine.

When you want to know whether your working tree is slower than `main`:

```bash
make bench-compare BASE=main
```

You should see one table per benchmark executable; negative `Time` values are faster.
The baseline builds in `build/bench-base/`, so the next comparison against the same ref is incremental.

## Hardware counters

The pinned vcpkg `benchmark` port has no libpfm feature, and vcpkg has no libpfm port.
When you want cycle and instruction counts on Linux, copy the port as an overlay:

```bash
sudo apt-get install -y libpfm4-dev && mkdir -p ports && cp -r .vcpkg/ports/benchmark ports/
```

Add `-DBENCHMARK_ENABLE_LIBPFM=ON` to `vcpkg_cmake_configure` in `ports/benchmark/portfile.cmake`.
Configure with the overlay, run `make bench`, then add `--benchmark_perf_counters=CYCLES,INSTRUCTIONS`:

```bash
cmake --preset bench -DVCPKG_OVERLAY_PORTS="$PWD/ports"
```
