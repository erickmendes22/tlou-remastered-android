# TLOU Android v0.28 — compute-queue stalls, not shader-FMA speculation

Date: 2026-11-04. Branch: android-bionic-stage1.

## Why the v0.27 hypothesis was wrong

On-device logs 17, 18, 19, 20, same Turnip Gen8 V37/720p/Readback 1:
steady combat ~35.4-36.7 frames/s across configurations and versions.
The v0.27 relaxed-FP experiment was ON in log 20 but didn't improve
sustained FPS materially (36.72 vs 36.38 v0.26, and previous unmodified
36.57-36.67). GPU wait counters still ~1.03 s/5 s and uploads ~25k
batches/5 s. Source `EmitFPFma32` already emits OpFma explicitly; omitting
NoContraction is not itself a new fused arithmetic instruction.
The two GCN dumps in archived ZIPs show many vector add/half operations
depending on the chosen GCN disassembler target; their exact compiled
Adreno machine instruction mix has not been verified. Do not pretend
removing one decoration measurably saved 14.5 ms in hardware.

The GPU command processor's CPU-thread clock is ~73-77% of a core in
steady gameplay. At ~36.5 FPS that is ~20.0-21.1 ms *CPU execution
per frame* on the GPU command thread. GPU tick waits are ~1.0-1.1 s
*over five seconds*, about ~5.5-6.0 ms per flip. The two quantities
may overlap and are not an exact frame-time partition. The resulting
~27.4 ms/frame matches a CPU-command/synchronization bottleneck more
closely than the 540p/720p-invariant shader hypothesis.

## Existing, measured PC-port candidate actually disabled on Android

File: `gow3_pc/gpu/shadps4/video_core/amdgpu/liverpool.cpp`.

In `ProcessCompute`, the default path calls `DrainDrawPipe` upfront
for **every** `WaitRegMem` packet and nested `IndirectBuffer`.
These don't always require any drawing work to finish:
- The existing guarded `FewerDrains` compute wait path, when enabled,
  uses `PendingFenceValue` and `TestValue` for stream-ordered already
  queued writes, checks `LandDeferredFenceFor` and deferred-label
  ownership, checks `HasPendingWrite` when pending fence tracking is
  disabled, then yields to other queues while the wait is genuinely
  unmet. It still drains when it must.
- The nested compute IndirectBuffer does not itself touch the renderer.
  Its **inside packets** still execute their own ordering/drain checks.
- The comments from prior same-title desktop experiments document
  1,200 compute wait drains/sec, only ~1% satisfied by drain, using
  16-18% of command-thread time, and 2,300 nested-IB drains/sec using
  10-28% on a laptop. **These are not measurements on the phone.**
- The general `GOW3_FEWER_DRAINS=1` has a **documented graphics
  readback race**: never enable globally on TLOU Android just to get
  its guarded compute path.

## v0.28 implementation

- `TLOU_COMPUTE_WAIT_FAST=1`: Android-launcher default **ON** for new
  and existing installations (unless user explicitly saved OFF).
  New single UI toggle under Experimentos, restart to apply.
  `0` restores old compute pre-drains.
- **Only `ProcessCompute`** uses this flag: skip *upfront*
  `DrainDrawPipe` for compute `WaitRegMem` and nested compute
  `IndirectBuffer`, then reuse the existing guarded and yield-based
  compute wait implementation. No custom fence prediction, stale data,
  asynchronous speculative guest writes, or removal of necessary
  GPU timeline waits.
- `ProcessGraphics`, `FewerDrains()`, readbacks (Relaxed 1),
  `Scheduler::Wait`, guest memory byte merger, CPU-write guard, shader
  resource bindings and image barriers are **unchanged**.
- Deterministic host unit test `test_tlou_compute_drain.cpp` covers all
  8 combinations of new flag, legacy fewer-drains and legacy
  nested-IB-only flag for WaitRegMem, IB and unrelated opcode. The
  source audit/test checks that only the two compute packet types
  can bypass the up-front predrain.
- Remove unproven relaxed-FP v0.27 source, menu and metadata variant;
  keep split64 option and its cache separation unchanged.
- Source build and GPU native compiler must pass in CI.

## How to evaluate — without repeating old OFF captures

The existing log 20 and earlier logs already capture the old default
compute-drain behavior (`GOW3_FEWER_DRAINS` and `GOW3_SKIP_IB_DRAIN`
were not set by PortSettings). Leave the new default ON for the next run;
same save/resolution/driver. Existing baseline heavy scenes had ~36 FPS.
Compare stable median FPS, 1% lows, command CPU%, GPU tick wait ms,
completion of compute physics queue/fences, rendering and save/load.
An FPS improvement **is not yet established**. If regressions occur,
disable the one compute toggle and do not ship it by default.

No additional experimental memory-mode, NoContraction, speculative
readback, or unmeasured global sync bypass is bundled.
