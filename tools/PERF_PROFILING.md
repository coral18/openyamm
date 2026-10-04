# Desktop CPU profiling with perf

Run these commands from the repository root. `build-prof` keeps release optimization and adds debug information
for source locations and sampled call stacks. It does not enable gprof instrumentation.

## Build

```sh
cmake -S . -B build-prof -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_C_FLAGS_RELEASE=-O3 -g -DNDEBUG' \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -g -DNDEBUG' \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DOPENYAMM_ENABLE_GPROF_SAMPLING=OFF \
  -DOPENYAMM_BUILD_TESTS=OFF \
  -DOPENYAMM_BUILD_EDITOR=OFF \
  -DOPENYAMM_BUILD_TOOLS=OFF
cmake --build build-prof --target openyamm -j25
```

The initial local setup copied the existing `build-gprof/_deps/*-src` dependency sources into
`build-prof/_deps/` and configured `FETCHCONTENT_FULLY_DISCONNECTED=ON` to build without downloads.
For a fresh build directory without those sources, leave that option off (the default).
FFmpeg uses the repository's separate configure/build procedure.

## Record New Sorpigal gameplay

```sh
./tools/profile_game.py -- --world mm6 --map oute3.odm
```

This launches `build-prof/game/openyamm` with ordinary settings/assets/saves and starts perf with counters
**disabled**. Enter/load New Sorpigal if the main menu opens. Collection starts only when the game's gameplay
screen has a loaded map, the outdoor sprite warmup queue is empty, and three additional seconds have elapsed.
Wait for `[Perf] collection enabled`, then play for 60–120 seconds and close the game normally to finish.
Stand still during warmup: the existing sprite warmup queue can defer work while the camera is moving.

Collection pauses before map loading, while the loading overlay or a separate screen is active, and at shutdown.
It resumes after gameplay is ready again, with the same warmup. The game waits for perf to acknowledge each
state change. This excludes map initialization and queued sprite preparation; assets requested later by actual
play remain part of gameplay costs. The existing gprof gate uses the same readiness check, while its headless
map-loading scope remains specific to gprof.

The result is `build-prof/perf-gameplay.data`. The wrapper uses 199 Hz userspace CPU-cycle sampling and DWARF
call stacks with a 16 KiB stack snapshot. It includes the game's threads. No sudo is needed with permissions
configured as below. Normal launches without this wrapper do not activate perf control.

Useful options before `--`:

- `--output build-prof/sorpigal-before.data`: choose a separate capture name.
- `--warmup 10`: wait ten seconds after each return to ready gameplay (`0` starts immediately when ready).
- `--include-loading`: record the entire process, including startup, asset loading, menus and shutdown;
  gameplay gating and warmup are disabled in this mode.

Use the same route, settings, resolution, VSync and warmup when comparing changes. This wrapper runs the
profiling executable directly; `tools/run_game.sh` continues to run the ordinary development executable.

## Inspect or export

```sh
perf report --no-inline -i build-prof/perf-gameplay.data
perf report --stdio --no-inline -i build-prof/perf-gameplay.data \
  > build-prof/perf-gameplay.txt
perf report --stdio --no-inline --no-children --sort comm,pid,dso,symbol \
  -i build-prof/perf-gameplay.data > build-prof/perf-gameplay-threads.txt
```

These reports include sampled call stacks and inclusive costs (a function together with its callees).
The second text report separates threads and shows self costs (`pid` is perf's command/TID sort key).
`--no-inline` avoids addr2line warnings observed on this machine during inline expansion; it does not disable
stack capture. Source/debug information remains in the executable. These are sampled CPU costs, not exact
call counts, elapsed frame times, or GPU execution measurements. Shared-library symbol detail depends on the
symbols available for those libraries. Check the recording output for lost samples or unwinding warnings.

Keep the `.data` file and the matching executable/debug information until analysis is finished; analyze or
archive them before rebuilding. `build-prof/` is ignored by Git. Use different output names for comparisons.

## Prerequisites

On Ubuntu, install `linux-tools-common` and `linux-tools-$(uname -r)`. `perf version --build-options` should
show DWARF/unwind support. If profiling is restricted, `sudo sysctl -w kernel.perf_event_paranoid=2` allows
own-process userspace profiling until reboot. The commands above explicitly select userspace events.

References: [perf record](https://man7.org/linux/man-pages/man1/perf-record.1.html),
[perf permissions](https://www.kernel.org/doc/html/latest/admin-guide/perf-security.html).

## Validation

The optimized profiling build passed. A native perf integration check captured two gameplay intervals with
no loading/warmup samples and zero lost samples; `--include-loading` captured all three phases. The actual
New Sorpigal headless map-loading diagnostic completed in both modes: default gating produced zero CPU samples,
and loading mode produced readable game call stacks. This verifies collection control, not desktop GPU performance.
