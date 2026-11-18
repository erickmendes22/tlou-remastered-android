# TLOU Android v0.24 — adjacent Vulkan copy batching (opt-in)

The user's TLOU v0.23 captures have ~23,768–25,602 CPU-written-buffer
upload batches per five-second heavy-scene window (roughly 130–145 MiB).
TLOU_INLINE_SINGLE_COPY was already enabled and handled ~22,000–24,000
small one-region transfers in the matching period. These are counts, NOT
evidence that all uploads have adjacent Vulkan commands, or an FPS claim.

## Change, correctness, and A/B experiment

* A new TLOU_COPY_BATCH=1 switch (disabled by default) replaces the
  per-copy recorder closure with an explicit RecordChunk copy command
  when copying one region of at most 64 KiB and threaded recording is active.
* Only immediately neighboring copy commands with identical source and
  destination Vulkan buffer handles can merge. The source and destination
  buffers must be different. Destinations must be nonoverlapping; copy
  sizes/offsets are overflow-checked. At most four regions per Vulkan command.
* Any other recorded operation (including Vulkan barriers, rendering,
  dispatch, or draw) breaks the group. All IsBufferAccessed, AccessBuffer,
  FlushBarriers, upload staging, host-copy fences, readback safety, and
  texture invalidation continue unchanged. The stream of guest-visible
  memory updates is not modified.
* A/B report records the new toggle. When enabled, the recorder reports
  "TLOU copy batch (>=5s): copies=N merged=M Vulkan-calls=N-M".
  If M is small or zero, it has poor real-game coverage and must be
  removed rather than retained as another useless option.
* The previous descriptor_bulk v0.23 experiment showed no FPS gain in
  the same heavy scene, is forced OFF, and is removed from the menu.
* Use the same save, scene, duration, resolution and options as the
  earlier v0.23 logs, leaving TLOU_INLINE_SINGLE_COPY=1 in both runs.
  Profile capture should be disabled in the timed comparison.
  Keep the experiment only if median and low-percentile FPS improve
  with no missing geometry, corruption, crashes, or readback regressions.

This is a targeted **unproven** optimization based on an observed large
number of copy commands; native CI passing does not prove Android FPS gain.

Native unit test: android/tests/test_copy_batch.py (adjacent ranges, overlap,
overflow and empty ranges), plus Android native APK compilation.
