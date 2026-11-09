# TLOU v0.18 — Small buffer-copy recording experiment

## Evidence from the heavy scene
At approximately 36 FPS in Nov 4 logs, the stable scene generated
24–25k upload batches (140–145 MiB) every five seconds, and around
1.0–1.1 s of GPU tick waits. The waits overlap other work and cannot be
removed blindly. Old DMA batching did not demonstrate FPS improvement.

## Implementation
TLOU_INLINE_SINGLE_COPY=1 captures a single vk::BufferCopy region of up to
64 KiB by value in the deferred command closure. The old
Scheduler::RecordData reserve + memcpy is avoided. The exact Vulkan
copy command, dirty-page selection, staging buffer lifetime, barrier
tracking, EndRendering, waits, and command order are preserved.
Multi-region and larger copies take the original path.
An OFF-by-default menu toggle controls the change.
Five-second counters report eligible and actually inlined copies.

A native test verifies size/count limits and deferred lifetime after the
caller's transient region array is mutated and destroyed. The experiment
may reduce CPU-side command-recording work but cannot reduce upload bytes
or mandatory GPU/CPU waits; device FPS improvements are unverified.
Compare ON and OFF in the same scene, GPU profiler OFF, keeping 720p,
Readback 1, Turnip, and the other experimental settings fixed.


## Second independent experiment: stack snapshot for tiny GPU-to-CPU mapped reads

The heavy scene records ~18k direct mapped SRT readbacks every five seconds;
their total is only about 2.5 MiB, or roughly 140 bytes per readback.
The original path allocates a new heap vector for every snapshot. With
TLOU_STACK_MAPPED_READBACK=1, snapshots of 1–512 bytes instead use a
512-byte stack array. Larger reads and disabled mode still use the old
heap vector. The input PlanMappedReadback validation, original wait for
the latest producer GPU tick, CopyMappedReadback, WriteBackGpuData and
all page/alias/newer CPU write handling are exactly the same.

A separate OFF-by-default menu toggle enables this path. The existing
mapped readback five-second line reports stack-small=N, so a device A/B
can prove it executed. The original randomized mapped-readback tests,
the guarded PROT_NONE wait-before-read test, and new threshold tests
are run in CI.

This is a CPU allocation optimization, not elimination of mandatory
GPU work, readbacks, or synchronizations. Compare either experiment
individually and keep disabled if no stable FPS improvement.
