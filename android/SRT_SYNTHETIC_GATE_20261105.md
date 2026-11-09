# SRT same-page candidate: synthetic cost gate, 2026-11-05

## Decision

**Do not deliver cc9e447's APK as a supported performance improvement.**
That commit enabled same-page read-ahead without showing a reduction in its
claimed blocking bottleneck. It is now explicit opt-in only with
`TLOU_SRT_PAGE_AHEAD=1`; it no longer activates silently. No additional APK
is requested by this change (`[skip ci]`); source and tests are retained.

## Reproducible test

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  gow3_pc/tests/test_tlou_srt_cost.cpp -o /tmp/tlou-srt-cost
/tmp/tlou-srt-cost
```

The runtime contract script also runs this test before future native builds.
It executes production `PlanMappedReadback`, `CopyMappedReadback` and the
late publication gate. The dirty-range discovery, ownership flags, residency,
timeline completion and guest memory are modeled. It does **not** execute
full `BufferCache::DownloadRanges`, Vulkan driver, Adreno or the game. CPU
timings therefore describe this harness only, not Android performance.

The fixture preserves the first 16 SRT sync addresses and sizes in
`tlou/runtime.log` from `TLOU-Android-Logs (23).zip`, SHA256
`3892e329029b26bf34c6f40f1ec1459943b814ec2318ce916c991d05aaa51106`.
The actual size at `0x11446c32c0` is 64 bytes and at `0x11446c3f80`
is 16 bytes. The log lacks GPU write times, simultaneous ownership,
resident layouts, and the full gameplay request sequence. Initial ownership
of every unique field is a deliberately optimistic assumption, not a fact
established by that log. The repeated exact address `0x11446a3a00` is
modeled with a new GPU write rather than counted as a free cache hit.

## Deterministic results

Each result compares exact-only baseline / same-page candidate. The stall
column is **virtual timeline units**, not measured GPU microseconds. No
sleep, fake fixed overhead per wait, or asserted FPS multiplier is used.

| Scenario | Readback/wait calls | Blocking waits | Modeled stall | Bytes copied |
| --- | ---: | ---: | ---: | ---: |
| Two fields on same page, one producer | 2 / 1 | 1 / 1 | 1000 / 1000 | 80 / 80 |
| Log 23, optimistic ownership | 16 / 14 | 2 / 2 | 1100 / 1100 | 1228 / 1228 |
| Three unrelated pages | 3 / 3 | 1 / 1 | 1000 / 1000 | 192 / 192 |
| GPU rewrite before next field is used | 2 / 2 | 2 / 2 | 1200 / 1200 | 80 / 96 |
| CPU write during first wait | 2 / 2 | 1 / 1 | 1000 / 1000 | 80 / 96 |
| Extra dirty field never consumed | 1 / 1 | 1 / 1 | 1000 / 1000 | 64 / 128 |

All cases assert requested-byte equivalence. Late CPU writes are preserved;
rejected candidates remain dirty. The test asserts that API-call reduction
is **not** automatically credited as a blocking-time reduction.

An initial x86-64 host run, median of 9 samples of 3000 replays after warmup
and alternating OFF/ON order, gave **2565 / 2861 ns per replay** (ON/OFF
ratio **1.116**). This is a negative result in the harness, not proof of an
11.6% regression on the device. Timing is reported rather than used as a
flaky CI threshold. Preserve repeated outputs before interpreting differences.

Three further independent runs produced baseline/ahead medians of
2469/2610, 2648/2789 and 2527/2730 ns. All four host-harness runs were
slower with ahead enabled (ratios 1.053–1.116); none establishes a
repeatable CPU saving, even with optimistic initial ownership.

## What this rules out

The two fields share the same page/window producer tick. Baseline's first
read already waits until that producer completes. The second check can
return immediately. Combining them may eliminate lookup/planning/copy-call
overhead, but does not itself overlap producer execution or shorten the
critical GPU dependency. The previous claim that repeated-page addresses
alone support a reduction of the ~1.09 s accumulated wait is unsupported.

## Gate for another candidate

1. Run the same production function or a shared helper in baseline and
   optimized forms; label every simulated dependency and missing stage.
2. Assert identical observable bytes and ordering for ordinary, no-reuse,
   rewrite, pending-readback and late-CPU-write cases.
3. Count operations actually removed and identify whether they are blocking
   waits, already-completed checks, submissions, bindings or allocation work.
4. Measure CPU cost with warmup, alternating order and multiple samples.
   Require repeatable improvement on the targeted production hot path before
   requesting a device APK; no automatic pass from fewer calls alone.
5. Only claim GPU-stall reduction when the dependency schedule demonstrates
   it, then validate it in matched device gameplay. Synthetic tests cannot
   guarantee Android FPS, cache hit coverage or the runtime critical path.

This candidate has **not** passed the cost/coverage gate. The synthetic test
itself passes because it detects and preserves that negative result.
