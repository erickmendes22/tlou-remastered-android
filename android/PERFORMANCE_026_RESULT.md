# v0.26 on-device outcome — CP NOP fast path retired (2026-11-04)

Measured user logs: TLOU-Android-Logs (17)(1).zip,
TLOU-Android-Logs (18).zip, TLOU-Android-Logs (19)(1).zip.

**Do not ask for an additional OFF test.** The runtime in log 18 is
TLOU Android v0.25 with CP NOP fast *absent*, GeneralEpoch OFF,
Turnip Gen8 V37 patched, 720p, Readback 1, unchanged other options.
The v0.26 runtime and previous runtime in log 19 both report
TLOU_CP_NOP_FAST=1; they are two ON runs, not an ON/OFF pair.

Comparison filters: frame summary <45 FPS, >=22k upload batches in
the matching 5s transfer period, zero shader compiles, low pending
draws, no startup transition. Raw v0.25 OFF log 18 stable heavy
windows: 35.4, 35.6, 35.8, 35.6, 34.8 -> mean 35.44.
v0.26 ON log 19 stable heavy windows:
37.0, 36.2, 36.3, 36.0 -> mean 36.375.
v0.26 ON log 19 previous stable heavy windows:
36.3, 36.3, 36.3 -> mean 36.30.
Additional reference: v0.25 log 17 with GENERAL ON but 0 barriers
skipped, stable windows: 37.3, 37.2, 36.3, 36.5, 36.6, 36.8, 36.0
-> mean 36.67, *higher than v0.26 with NOP ON*.
v0.25 log 18 previous, GENERAL ON but 0 barriers skipped:
36.1, 36.1, 36.3, 36.2, 36.1, 35.7, 35.6 -> mean 36.014.

The isolated old OFF log18 to new ON log19 is +0.935 FPS (+2.64%),
but baseline variation includes an older no-NOP run at 36.67.
This is NOT an experimentally supported CPU fast-path gain; release
and thermal effects are not controlled.
The log 19 earlier frame lows 19.7/24.0 and current 29.2 are
startup/scene transitions, not comparable steady-state samples.

Source-coverage telemetry for new code is real:
1.79–1.85 million skipped NOP packets per ~5 seconds, in
1.67–1.73 million batches; ~1.073 packets per batch.
This means virtually all skipped commands are isolated. The
v0.26 implementation used two atomic fetch-adds per batch,
creating substantial reporting overhead in the fast path.
Despite huge counts, CPU command utilization in the current
log 19 stabilized near 76.4% of one core; log18 OFF was ~74.8%
(log17 prior about 74.7%). No clear time saved. The 5-second
FPS and command CPU metrics do not justify shipping this as an
optimization, even if some commands were in fact skipped.

Underlying work: ~24–26k GPU upload batches/5 seconds
(~140–149 MiB), 18–19k direct mapped readbacks/5 seconds
with no staging fallback, ~1.0–1.1 seconds cumulative GPU-tick
waits per 5s. These are instrumented *overlapping waits*, not
GPU utilization; must not subtract directly from FPS time.

Decision: revert the v0.26 CP NOP implementation and launcher
option in source, retaining native GPU LTO and the functional
readback and memory guards. Do not trigger or distribute a
new APK just to remove a non-improving experiment. The last
published APK remains v0.26; this commit is source cleanup
for the next substantive change.

Next research should target proven PC/shadPS4 work-elimination
schemes with actual TLOU coverage, particularly draw-state/
descriptor generation or CPU page/texture upload costs. Do not
copy SotC FastBind/DrawReplay blindly: TLOU already has a
RenderStateMemo and TextureSetMemo; prove new cache hits
and resource correctness before skipping work.
