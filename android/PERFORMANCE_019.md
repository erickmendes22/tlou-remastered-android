# TLOU v0.19 — cross-port readback comparison and guarded experiments

## Device evidence (Nov 4, 720p, Readback 1, Turnip V37)
The three latest heavy-scene runs contain stable FPS near 35–36, approximately
24k GPU upload batches / 140 MiB per 5 seconds and 1.05–1.09 seconds of
instrumented GPU tick waits per 5 seconds. Small-copy and stack-readback
optimizations from v0.18 are actually used but show no established FPS gain.

## Review of six sources
1. PC TLOU / GoW III GPU runtime has the already ported GOW3_FETCH_NO_BLOCK
   mechanism: a guest write fault can wait on its own thread for an existing
   submitted GPU write, then recheck the page. Maximum three retries, with
   original fallback. It was not exposed in the Android menu; now opt-in.
2. Shadow of the Colossus PC has SOTC_RB_NARROW: a fault readback window
   is reduced only in Precise mode (where GPU pages outside the window remain
   read-protected). The original window is 512 KiB. The new TLOU option
   selects 64 KiB, but leaves the entire original readback/synchronization
   and page-merge path untouched. In Relaxed mode it is a no-op.
   SotC's separate async stale-interval engine relies on game-specific
   ownership tracking and is not transplanted blindly.
3. GoW3 Android shares the basic page-aware buffer cache but has different
   GPU/driver compatibility fixes; no broad copy of its allocator.
4. inFAMOUS Second Son PC and Android have deferred/eager readback page
   provenance requiring writer tracking. TLOU already has mapped readbacks,
   GPU-range ownership and lazy-page merge; no unguarded reuse of ISS writes.
5. Uncharted Android uses an earlier GPU buffer-cache architecture than
   this TLOU branch and lacks some protections already present; do not
   replace more recent TLOU routines with it.
6. Bloodborne native/ARM ports use a dedicated readback queue where
   supported. Mobile driver queue families and ownership transitions
   differ; this path cannot be assumed safe on Adreno.

## Changes, both OFF by default
- TLOU_SOTC_PRECISE_NARROW=1: only with Readback 2, choose 64 KiB rather
  than 512 KiB aligned window for full GPU data readback after a guest
  fault. Existing dirty-page filtering, barriers, merges, protection
  changes and waits are retained. The rest stays marked GPU-modified.
  Every requested byte remains in the selected window. Includes native
  boundary/containment regression tests.
- GOW3_FETCH_NO_BLOCK=1: expose existing guarded GoW3 wait relocation
  for guest CPU write faults; limited to safe submitted producer ticks,
  compatible copy queue, maximum three retries and original fallback.
  No effect if the driver has no suitable copy queue.

These are optional experiments, not proven Android FPS improvements. Keep
all existing settings, including Readback 1, unchanged unless deliberately
testing Precise. Compare one option at a time, GPU profiler OFF.
