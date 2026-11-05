# TLOU Android v0.26: source-grounded PC comparison and measured v0.25 A/B

**Device/source captures:** TLOU-Android-Logs (17)(1).zip and (18).zip,
2026-11-04, Snapdragon 8 Elite, resolution=720, Turnip Gen8 V37 patched.
The previous run included in (17) uses a different Turnip 27.0.0 r3
and v0.24, so it is not a valid A/B reference for LTO.

* 17 runtime: TLOU_GENERAL_EPOCH_SYNC=1. 19 epoch reports, total
  eligible=29 skipped=0; last 5 heavy windows 36.44 flips/s.
* 18 runtime-previous: GENERAL=1. 20 reports, total eligible=17
  skipped=0; last 5 heavy windows 35.98 flips/s.
* 18 runtime: GENERAL=0. Last 5 heavy windows 35.44 flips/s.
* Differences of ~0.5–1 FPS between different runs are not controlled
  enough to attribute to the toggle. **Zero barriers were elided**.
  The SotC PC 0.60.6-plus5 GENERAL optimization (53–54 -> 59–61 FPS
  in its Shrine) has no real-world coverage in the TLOU heavy scene
  under the conservative equivalent guard. Remove it.
* About 24–25k GPU upload batches (~140 MiB) and 18–19k direct mapped
  readbacks (~2.5 MiB) per five-second heavy window. No staging
  fallbacks on direct reads; about 1.0–1.1s cumulative tick waits per
  five seconds. Guest readback guards still preserve 16-byte CPU writes
  in shared pages. Not safe to drop GPU producer waits.
* command CPU ~73–77%, draw CPU ~60–65%, recorder ~50% of one core
  each. Main ~0.1%; no shader compiles in stable heavy scene.
  Thus optimize GPU command / draw processing, not shader compilation
  or raw readback staging.

## Actual PC/fork code surveyed

1. TLOU PC PGO+LTO (gow3_pc/build.sh): ~9.8% gain on x86_64 *with
   PGO and LTO together*. Android v0.25 enables only LTO; current
   captures do **not** independently prove an FPS increase.
2. SotC PC GENERAL barriers: v0.25 port implemented but 0 skipped,
   removed here.
3. SotC PC DepthFastPath / FastBind: TLOU already implements
   RenderStateMemo, TextureSetMemo, prepared sharps and a draw-pipe.
   The SotC algorithms cannot be copied on top without new generation
   invariants: texture/backing, DMA writes, guest CPU changes and
   barriers. A duplicate wrapper is not an optimization.
4. SotC PC DrawReplay (+8.6% Shrine / ~24% desert route) depends on
   FastBind and deterministic guest-state generations. No matching
   TLOU repeatability verification exists; not safe to skip draw work.
5. SotC PC CpFastPaths: checked the actual IsInertNop predicate in
   gpu/shadps4/video_core/amdgpu/liverpool.cpp. The TLOU command
   processor similarly sends no-op PM4 packets through its general
   opcode/command processing loop. Port the **safe, isolated** NOP
   skipping subset to android-bionic-stage1.

## v0.26 change

* `TLOU_CP_NOP_FAST=1` (launcher, OFF default): skip at most 64
  consecutive inert type-3 NOP packets per loop iteration, with
  whole-packet length validation. Any PatchedFlip, command with debug
  marker capture enabled, invalid type or truncated packet takes
  the old path. At least every 64 packets ProcessCommands() runs.
  After any guest-visible fence, the existing DCB_COPY_ON_SIGNAL code
  still clones the rest of the command buffer before NOP parsing.
  This preserves the TLOU-specific early command-buffer rewrite
  mitigation absent from the PC fork.
* Periodic counters: "TLOU CP NOP fast (>=5 s): skipped=N batches=M".
  If N=0 or CPU/FPS do not improve, remove the experiment.
* Unit test: android/tests/test_cp_nop_fast.py.
* The v0.25 GENERAL barrier experiment and all its image state
  modifications are reverted. Keep native GPU LTO because it links
  successfully; measure it separately before claiming benefit.

A/B: same APK, same save/area, all other toggles unchanged, NOP Fast
OFF vs ON, GPU profile OFF. The original fork's CP improvement came
from multiple independent fast paths, so **do not attribute a 5%
speedup to this NOP subset**.

Sources:
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/gpu/shadps4/video_core/amdgpu/liverpool.cpp
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/README.md
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/docs/sotc/DISPATCH-FASTPATH.md
