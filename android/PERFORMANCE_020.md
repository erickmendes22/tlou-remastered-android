# TLOU v0.20 — Relaxed only, three independent memory/performance experiments

## Removal of problematic Readback 2
The user observed graphical bugs and lower frame rates with Readback 2.
The TLOU launcher now **always** sets GOW3_READBACKS=1 regardless of
resolution or any legacy saved readback setting, and clears old readback
and SotC precise-window preferences. Both the UI selection and its
Precise-only 64 KiB fault window were removed completely; the previous
512 KiB fault readback path was restored in the renderer. This does
not change the other existing mapped-readback and lazy-page protections.
The core's generic Precise enum is retained for other games and internal
reasons but is no longer selectable through the TLOU launcher.

## Remaining/new opt-in opportunities in Relaxed
1. GOW3_FETCH_NO_BLOCK (from v0.19, OFF by default): existing guarded
   relocation of eligible GPU writer wait from command thread to guest
   faulting thread, only after producer submit, at most 3 retries then
   original fallback; requires a compatible copy queue.
2. GOW3_LAND_ON_GUEST (new exposed toggle, OFF): existing GoW3 guarded
   guest-thread landing of an outstanding page readback only when the
   producer tick is **already submitted**. Poll on command thread,
   wait the submitted timeline on the faulting guest thread, then
   re-check; after four attempts or missing producer, fallback to
   original command-thread landing. Retains GPU ownership and barriers.
   Only helps when read-watch landings occur, so may be a no-op in a
   scene with none.
3. GOW3_STATIC_MIN_BYTES=512 (new exposed toggle, OFF, old default 1024):
   lowers the lower bound for promoting small read-only stream buffers
   into the resident arena. Uses the preexisting static-buffer
   guards: observed for three distinct submission ticks, not kept
   writable, no untracked backing write, no GPU write into those pages,
   and demotes immediately when CPU modification is tracked. The option
   cannot reuse ordinary dynamic GPU-writable buffers.

The third-party engines compared were SotC PC, GoW3 Android/PC, ISS PC
and Android, Uncharted Android/PC, and Bloodborne ARM. More invasive
readback queues, stale-interval async snapshots, unprotected pages,
and full range ownership conversions were **not** ported because they
would bypass TLOU's memory-protection/lazy-merge model.

## Baseline and experiment strategy
Nov 4 v0.18 heavy-scene logs 3/4/5: 35–36 FPS, ~24k GPU
uploads (~140 MiB) and ~1.05–1.09 seconds in GPU tick waits per
five seconds; both prior v0.18 micro-optimizations executed thousands
of times without a verified FPS improvement. Do not claim gains from
these changes without the device A/B. Run one new toggle at a time on
the same scene, GPU profiler OFF and unchanged driver/resolution.

Most importantly: never enable Precise/Readback 2 to get a faster FPS.
