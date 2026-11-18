# TLOU Android v0.29 — temporal SRT readbacks at the documented CPU↔GPU stall

2026-11-04 | Source: `android-bionic-stage1` | Runtime driver: Turnip Gen8 V37 patched, Adreno 830.

## Evidence from existing user logs (no new OFF baseline required)

In the log 21 heavy-scene final five windows (720p, Readback 1), 34.8-37.3
FPS, command CPU 71.5-75.7% of a core and 1,008-1,095 ms spent waiting
for GPU timeline ticks every 5 seconds. Shaders compiled: 0. Approximately
15.4-18.4 thousand mapped GPU→CPU downloads for only 2.1-2.6 MB per
5-second window; ~99% avoided a new Vulkan submission but still had to
honor their producer ticks. Those costs did not change between log 20
(v0.27) and log 21 (v0.28); v0.28 compute-wait fast path was reverted
in source without a new APK.

The GPU-written resource tables consumed by
`Rasterizer::SyncSrtTables` cause separate calls to
`BufferCache::DownloadRanges` before flattening per-shader SRT
descriptors. Existing code explicitly attributes ~9.4k downloads/5s
to SRT in log 20. The earlier v0.21 same-walker independent sibling
batching found zero valid groups; it cannot merge tables from *different*
shader walks.

## External documented result and reason to port this mechanism

`Lucas-blip-png/ShadowOfTheColossus-PC`, `GatherDownloads` in
`gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp`:
SOTC_RB_HOT remembers recently demanded GPU-written ranges, then downloads
them alongside another mandatory wait when they share the same available
GPU synchronization. The PC fork reports **34.6→43.3 FPS** in a Shrine
idle comparison after repairing corruption in the initial version.
It also documents stale speculative copies corrupting textures and a
game heap before the late CPU-write guard was added.

This is evidence of the *mechanism in a related PC port*, **not** a
measured gain, guaranteed percentage or validated visual correctness
on TLOU/Adreno. The TLOU code differs in buffer arenas, readback
page protection, mapped HOST_COHERENT access and GPU write tick tracking.

## v0.29: conservative TLOU-specific implementation

Native environment `TLOU_SRT_HOT_READBACKS=1` (launcher default ON;
single toggle in Experiments, restart to apply) only changes SRT's two
call sites *after* `DrainDrawPipe`. Every other producer/consumer
path, including game CPU faults, asynchronous and EOP readbacks, retains
exactly its previous behavior.

At each *mandatory* SRT download:
1. Record actual requested SRT tables up to 512 bytes in a fixed 64-entry
   history (age at most 4 sec), rather than random GPU ranges.
2. Consider at most 16 candidate pieces/8 KiB in total and only from
   the same resident mapped buffer arena as the requested table. Drop
   candidate tables that are no longer GPU-written *before* expensive
   page checks. Require no guest CPU-modified bytes, lazy/poisoned pages,
   pending async readbacks, or overlap with **any mandatory or already
   selected 4 KiB page**.
3. The candidate's latest recorded GPU writer tick, checked for all
   intersecting pages and 1 MiB windows under the GPU range lock,
   must be known, no later than the primary table's newest tick and
   strictly earlier than the current unsubmitted tick. This reuses
   the primary's unavoidable timeline wait; **never waits for later
   GPU work or submits more work**.
4. Reuse the same existing `PlanMappedReadback`,
   `CopyMappedReadback` and `WriteBackGpuData` guest-byte merge.
   **After** waiting/copying but **before** writing a speculative byte
   to guest RAM, check CPU modification, pending older readbacks,
   ownership by GPU of the entire range and new lazy pages. If any
   changed, **do not publish**; keep its GPU-modified range so a later
   requested readback follows the unchanged safe path.
5. Retire GPU-modified bytes and tracker page bits **only after**
   successful publication. Page bits clear only if no GPU-written
   interval remains in the page. Never subtract a skipped hot piece.
6. If a speculative mapping cannot be safely planned, drop **all**
   speculative pieces and retry the *exact* original primary-only
   mapped readback before ever using staging.
7. Preserve normal requested SRT readback ordering and independent
   parent→child traversal. No changes to shader code, resolution,
   render-state memo, Readback 1 or CPU write protection.

The SRT stage code explicitly suppresses temporal prefetch during
frame capture and shader tracing. This avoids introducing speculative
memory activity into diagnostics.

## Correctness tests and observability

`test_tlou_hot_srt_readback.cpp` tests: no unknown producer ticks,
no future/latest producer, bounded bytes, disjoint 4 KiB pages, CPU
writes after a wait, older readbacks, lazy pages and GPU-ownership
loss. Source relies on the existing mapped readback and readback
merger tests; native CI must compile the complete ARM64 GPU.

The existing low-overhead 5s summary remains unchanged. New line:

`TLOU SRT temporal readbacks (5 s): requested=N candidate-bytes=X published-bytes=Y late-skipped-bytes=Z`

No profiler/GPU timestamps or per-draw diagnostic pass needed.
The log will establish whether temporal overlap genuinely exists in
this user's heavy scene, a coverage fact neither v0.21 nor v0.28
measured. The acceptance gate remains **same-scene actual sustained
FPS and 1% lows, CPU%, timeline waits, images/physics/save-load**.
No claim of an FPS increase until the new device log shows it.
If published=0, the source hypothesis fails in this scene and the
feature must be retired, not followed by more random APKs.

Main references:
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/README.md
- `android/CROSSPORT_PERFORMANCE_EVIDENCE.md`
- `android/PERFORMANCE_028_RESULT.md`
