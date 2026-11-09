# TLOU Android v0.25 — actual PC/fork integrations (2026-11-04)

## Grounded candidate selection

**TLOU PC PGO+LTO:** gow3_pc/build.sh documents the heavy-area Windows
comparison 39.6 -> 43.5 FPS (+9.8%) with an x86_64-trained LLVM profile
PLUS link-time optimization. The Android build script had GOW3_LTO=OFF.
v0.25 turns on the GPU library's existing CMake IPO/LTO build machinery
and fails configuration if the requested Android LTO is unavailable.
This compiles into libgow3gpu.so (no other FEX/native translator change).
GOW3_PGO=off remains unchanged: the Windows profile must NOT be reused
on Android. TLOU_ANDROID_LTO=OFF is the build-time rollback switch.
No claim that LTO alone reaches the combined PC improvement.

**SotC PC 0.60.6-plus5:** Lucas-blip-png/ShadowOfTheColossus-PC reports
53-54 -> 59-61 FPS in the Shrine from GENERAL->GENERAL storage-image
barrier suppression per guest fence epoch. The actual source is:
- gpu/shadps4/video_core/texture_cache/image.cpp SkipSameGeneral
- image.h general_epoch
- vk_rasterizer.cpp Rasterizer::OnFence
- amdgpu/liverpool.cpp call sites for EOS, EOP, ReleaseMem, labels.

TLOU image.cpp still issued a same-GENERAL write barrier for every draw.
Ported behind independent TLOU_GENERAL_EPOCH_SYNC=1 (default 0) and
exposed at Video e desempenho > Experimentos. The TLOU fence equivalent
is Rasterizer::ProcessDownloadImages after DrainDrawPipe, reached by
the EOP/EOS/release paths in TLOU's liverpool.cpp. The counter increments
there in command-stream order, before the download processing.

**Additional safety guard beyond SotC:** suppression applies only to
full-image transitions with exactly the same GENERAL layout, access
mask and pipeline stage. The first same-GENERAL write barrier after
each fence is preserved. Any partial/subresource transition, changed
access, stage or layout resets the backing's barrier epoch; backing
replacement starts with an invalid epoch. Readbacks, fences,
texture/image transitions and GPU->CPU guest write merging unchanged.
SotC's assumption that writes don't depend on a barrier between two
draws without a guest fence is GAME-SPECIFIC; the TLOU option is OFF
until visual validation. It is not a correctness guarantee for TLOU.

When enabled, logs print:
TLOU GENERAL epoch (5 s): eligible=N skipped=N barriers-kept=N
The result determines whether there is real coverage, unlike v0.24
copy batching where 24,576 eligible copies produced ZERO merges.
Host unit test covers first use, repeated use, new epoch, changed
stage/access/layout, partial ranges, new backings and disabled switch.

## Do not blindly copy other published PC improvements

- SotC 0.60.6-plus13 DrawReplay (+8.6% Shrine, ~24% desert):
  depends on its FastBind cache, fast-bind keys and readback interval
  generations. TLOU's indirect draw passes through a separate
  record path, and its prepared-stage/draw cache is different. Copying
  the skip would bypass crucial texture invalidation, DMA and vertex
  binding effects. First establish repeatable *TLOU* work and source-
  level generation guards.
- SotC 0.60.6-plus4 hot speculative readbacks (+14% Shrine):
  TLOU already maps ~18k direct readbacks per 5 s without an extra
  GPU submission. The user's previous Readback guard kept 16 guest-
  rewritten bytes. A speculative landing risks overwriting game data,
  as the SotC fork itself documented in older versions. Not ported.
- SotC 0.60.6-plus26 worker naps (+19% on four threads):
  inapplicable to Snapdragon 8 Elite without evidence of contested
  CPU affinity. TLOU already uses separate GPU command, draw and
  record workers. Not ported.
- SotC 0.60.6-plus3 descriptor reuse (+8% Shrine):
  TLOU's simpler descriptor-bulk-copy experiment was already tested
  and showed no benefit; full descriptor reuse additionally needs
  resource-generation, upload, layout and visibility invariants.
  Do not assume same coverage or correctness. Not ported as a blind
  shortcut.

## One build, two comparisons

A: same heavy area v0.24 (old linker), general option absent.
B: v0.25 with GENERAL option OFF. B vs A gauges Android GPU LTO
(with version/build drift caveat).
C: same v0.25 APK, GENERAL option ON. C vs B gauges barrier elision.
Leave GPU profile collection OFF when comparing FPS. Inspect both
FPS distributions and visuals (shadows, smoke, geometry, save/load)
and the eligible/skipped counters. Remove the option if coverage 0
or rendering regresses. Never claim a measured gain from native CI.

Sources:
- https://github.com/erickmendes22/tlou-remastered-android/blob/main/gow3_pc/build.sh
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/README.md
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/gpu/shadps4/video_core/texture_cache/image.cpp
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/docs/sotc/DRAW-REPLAY.md
