# TLOU Android v0.28 — log 21 result, source rollback, next high-impact work
Date: 2026-11-04
Evidence: user-provided `TLOU-Android-Logs (21)(1).zip`, plus previously
provided log 20 (both contain the same prior-run runtime in
`tlou/runtime-previous.log`).

## Comparison — heavy scene, same phone / driver / resolution

* `launch.log`: Gen8 Turnip V37 patched; 1280x720, Readback 1;
  `TLOU_COMPUTE_WAIT_FAST=1`. Runtime confirms “guarded WaitRegMem and
  nested-buffer drain elimination ON”.
* `launch-previous.log`: prior v0.27 with
  `TLOU_RELAXED_SKINNING_FP=1`, no compute-wait opt. Other launch flags
  match aside from those deliberate version-specific changes.
* Exclude intro/60 fps windows and 27-29 fps loading transition; use
  the final 5 complete, compilation-free 5-second heavy-scene windows.
  The actual FPS sequence:
  - log 20 / prior v0.27: **36.4 37.2 37.1 36.9 35.6** => **36.64**
  - log 21 / v0.28: **37.0 37.3 36.5 34.8 36.4** => **36.40**
  - no durable FPS improvement; -0.24 fps (0.7%) is within scene/load
    noise, not a proven regression.
* Mean over same final five windows:
  - Command thread one-core CPU: **72.64% => 73.22%**
  - Draw thread one-core CPU: **60.52% => 60.04%**
  - GPU tick wait (overlapping instrumented cumulative wall time):
    **1034.14 => 1069.82 milliseconds per 5s**
  - Upload batches: **25071.6 => 25243.2 per 5s**
  - Shader compiles: **0** in every stable window.
* v0.28 final window direct mapped readbacks=16692 / ~2.32 MB,
  no-extra-submit=16534, staging-fallback=0, consistent with prior
  15k-19k mapped-readback calls. Readback 1 semantics unchanged.
* No VK_ERROR_DEVICE_LOST or native SIGSEGV in current log. Exit history
  shows package installs and remove-task, not evidence of a new crash.
  This does NOT prove rendering or physics correctness.

**Decision**: v0.28 compute queue fast path is NOT established as a
performance improvement. The flag changed packet-ordering checks but
did not materially change command CPU or FPS. Revert its source and
default-on launcher option; **do not build another APK for cleanup**.
The v0.28 installed APK may still have the option ON; user may switch
the toggle OFF without reinstalling. New source reverts it without
touching graphics or the pre-existing FewerDrains external debug path.
Do NOT claim the ~16–18% command-thread cost from an old desktop
experiment applies to Adreno phone; earlier experiments may already
have removed other costs and the log lacks a count of actual compute
drains bypassed.

## Source-level investigation: why the next step is not “faster copies”

`Rasterizer::BindResources` calls `BufferCache::PollReadbacks`, gathers
new `set_writes` per draw, and binds buffers and textures. The
`Pipeline::BindResources` code always schedules push constants and,
for the pushed-descriptor path, snapshots descriptor info and schedules
`vkCmdPushDescriptorSetKHR`. The optional bulk snapshot experiment
(v0.23) already failed to demonstrate an FPS gain; copying descriptor
arrays alone is not the target. `TextureSetMemo` and
`RenderStateMemo` already exist, but they do not reuse the complete
draw bind. `BufferCache::ObtainBuffer` also tracks guest/GPU writes,
readback ownership, and barriers; skipping these checks unsafely can
cause stale constants and visual glitches.

`BufferCache::SynchronizeMemory` produces one upload batch for
each dirty-region transaction and stages host-written bytes. 25k
batches per 5 seconds is real, but only ~140-150 MiB per five seconds;
number of batches does NOT prove 25k redundant GPU copies.
Prior v0.24 host copy batching joined **0 copies**. Do not introduce
a second unconditional batch implementation.

`BufferCache::DownloadRanges` is called by the resource table
synchronizer. The code comments attribute approximately 9400
synchronous downloads/5s to small SRT tables; direct mapped readbacks
avoid staging but **cannot eliminate waiting for the last GPU producer**.
The v0.21 independent-table batching yielded **zero compatible sibling
groups** on device. Readback 2 and unguarded GPU-to-CPU speculative
landing already broke rendering, and must remain excluded.

## Cross-port opportunity with measured external benefit, not a TLOU claim

`Lucas-blip-png/ShadowOfTheColossus-PC`,
`docs/sotc/DRAW-REPLAY.md`: measured with matched ABBA/BAAB,
state-repeat/FAST_BIND path improves FPS by **+2.74 +-0.76** in a
heavy CPU-limited scene and cuts **1.20 +-0.33 ms** of draw busy/frame.
The 0.60.6-plus13 bundled result was **+8.6% at the Shrine, ~+24%
in the desert**, but those are PC title scenes with their own CPU
and workload. TLOU has no equivalent complete FAST_BIND identity.
It would require a per-draw immutable/generation-checked descriptor
key, correct buffer/texture residency, GPU/CPU write epochs, aliasing,
feedback loops, prepared user-data generation and command-buffer
boundaries, with full-bind fallback and descriptor equivalence checks.

Do NOT generate a public APK until the replacement proves on a
reproducible workload that a significant fraction of TLOU draws can
take the guard-safe path. No stable-gameplay draw-repeat rate or
`BindResources` per-phase timing was logged in log 21, so claiming
this change will yield a particular FPS is unjustified. Preserve
the logged baseline, turn on any future verification only to diagnose
a real candidate, not for an empty build cycle.

References:
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/docs/sotc/DRAW-REPLAY.md
- `android/CROSSPORT_PERFORMANCE_EVIDENCE.md`
- `gow3_pc/gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_common.cpp`
- `gow3_pc/gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp`
- `gow3_pc/gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp`
