# v0.30: validated checksum plus bounded CPU phase sampling

The PM4 checksum change is already validated on native ARM64, but its share
of frame cost is unknown. Logs 22/23 provide thread totals and waits, not
per-function CPU attribution. Do not turn the checksum microbenchmark's
41–65% primitive saving into an FPS prediction or claim a frame-level win.

## Why more attribution is necessary

Two queue candidates failed to show stable host end-to-end gains; SRT
same-page prefetch also failed its cost gate. They remain excluded. The
next decision needs CPU time inside actual graphics and compute paths,
rather than another assumption about cache hit rates or GPU-wait counts.

## Bounded measurement

`tlou_cpu_phase_sample.h` is shared by production and standalone tests.
There are four independent thread-local roles:

- `command-draw`: entry, pipeline selection, submit-draw (packet preparation when pipelined).
- `command-dispatch`: entry, compute pipeline selection, submit-dispatch (packet preparation when pipelined).
- `draw-record`: entry, render state, resource setup, buffers, textures, draw commands.
- `dispatch-record`: entry/HLE, resource setup, buffers, textures, dispatch commands.

Each role attempts at most **8 samples in a one-second budget window**.
Sampling opportunities have a reproducible varying interval of 128–383
calls, reducing the fixed-period aliasing of selecting every Nth draw.
Accepted samples are also at least 125 ms apart, spreading them through
the second instead of consuming the budget in the first few frames.
No timestamp or GPU query runs on ordinary unsampled draws. The selected
sample uses `CLOCK_THREAD_CPUTIME_ID`; descheduling and waits are excluded.
At most 32 phase markers and 34 CPU-clock reads per sample are allowed.
Excessive markers or a failed/regressing clock discard the sample. Across
four roles the absolute ceiling is 1088 CPU-clock reads per budget window;
normal stage counts are substantially lower. A short wall-clock poll at
each sampling opportunity implements the rate limit and five-second report.

The sampler defaults ON on Android and OFF elsewhere. Internal environment
`TLOU_CPU_PHASE_SAMPLES=0` disables it completely, including all clocks.
It does not enable `GOW3_GPU_PROFILE`, GPU timestamps, frame capture, shader
tracing or the previous expensive profiling option. No new menu is added.

One summary per active role is printed about every five seconds, containing
total calls, completed/dropped samples, clock-read counts and **sampled mean
thread CPU microseconds** in each phase. It does not print a per-draw trace.

## Interpretation limits

These are sampled scope means, not exact frame costs. Samples can include
loading, skipped draws, HLE replacements and pipeline compilation; compare
steady matched gameplay. Phase timing includes timer overhead. A small
phase near the clock-call cost must not be treated as a measured bottleneck.
Callbacks running on the texture helper are not attributed to the caller's
CPU clock. Indirect draw/dispatch entry points are not independently sampled.
When draw piping is disabled, command and record scopes can nest on one
thread: **never sum the role totals as independent critical-path times**.
The constant per-role cap means this is a coarse attribution aid, not a
statistical guarantee of representative per-shader coverage.

## Tests before packaging

`test_cpu_phase_sample.cpp` verifies exact phase attribution with a fake
clock, the 8-sample limit over 100,000 opportunities, per-sample clock-read
bounds, report/reset, failed clocks, excessive markers, moved/finished
sample ownership, and zero clock calls when disabled. The real-clock
benchmark compares a bare loop, OFF and ON with the production thread-local
binding, 16 markers and a dependent work stream,
using warmup, alternating order and nine median samples. A near-zero or
negative measured difference is noise, not a claim of free/negative cost.
An additional budget case executes 34 real CPU-clock reads per selected
scope using synthetic wall progression, to measure the selected path's cost
without sleeping or hiding its clock density in a fast unsampled loop.
Native ARM64 and x86 CI run repeat benchmarks and ASan/UBSan checks.

Initial native ARM64 confirmation (`42012965454`, `856fcb3`) measured a
14.16 ns bare loop against 14.23/14.22 ns with sampling in two runs.
The selected path with the maximum 34 real CPU-clock reads cost
10.71/10.43 microseconds per sample. These are host microbenchmarks,
not total Android instrumentation overhead. An additional opaque-memory
boundary case prevents the compiler from hoisting the thread-local marker
checks across work, as might otherwise happen in a simple pure loop.

This build contains the accepted PM4 optimization and information needed
to choose the next larger CPU change. It does not claim sustained FPS
improvement until matched device gameplay establishes it. The current
normal Readback 1 path and retirement of unproven SRT prefetch are retained.
