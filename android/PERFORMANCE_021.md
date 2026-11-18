# TLOU Android v0.21 — dependency-aware SRT readback batching

## Evidence
The user's prior 720p Readback 1 tests stayed around 35–36 FPS.
The emulator reported roughly 18–19k *total mapped readback calls*
per five seconds with little recompilation in steady gameplay.
Those totals include sources other than SRT; they do not prove
18–19k blocked GPU waits. The current SRT implementation nonetheless
executes DrainDrawPipe + DownloadRanges separately for each GPU-written
resource table, including independent siblings in the same walker.

## Implementation: optional TLOU_SRT_DEP_BATCH
Off by default; Readback 1 remains the only available mode.
The native rasterizer collects at most eight independent resource-table
ranges or 64 KiB requested data per readback group.
- Stage B is drained at the beginning of each group.
- Before loading a child table's pointer from its parent, a pending
  parent group is fully downloaded and published to guest memory.
- If HLE copy writes are pending, a shader trace is active, the table
  exceeds the budget, or a group reaches its size limit, retain the
  individual original readback path.
- Existing BufferCache::DownloadRanges handles each group, so the same
  producer ticks, GPU barriers, page/alias tracking, mapped fallback,
  lazy page merges and guest writes are preserved.
- On-device counters report groups with two or more tables, table counts,
  and candidate DownloadRanges calls merged. Candidate calls are **not**
  necessarily saved GPU stalls.

The native host regression exercises parent-before-child ordering,
siblings with unrelated pointers, and hard count/byte limits.
Compilation and native test passing do not establish an FPS increase.
The previous baseline does NOT need repeating. For validation, compare
only the new independent SRT batch option on/off while holding all
other settings and the same loaded scene fixed; check the actual
group count and possible visual regressions before enabling by default.

No Readback 2 or unsafe stale/unprotected readback paths are introduced.
