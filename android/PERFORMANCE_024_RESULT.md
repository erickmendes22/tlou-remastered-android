# TLOU v0.24 — result and retirement (2026-11-04)

User capture: TLOU-Android-Logs (16)(1).zip (runtime.log, runtime-previous.log).
The launcher reported TLOU_COPY_BATCH=1; the hot scene showed repeated
"TLOU copy batch (>=5s): copies=24576 merged=0 Vulkan-calls=24576".
The copy-merging hypothesis is invalid for this workload. The same capture
showed TLOU_SRT_DEP_BATCH=1 with "groups=0 tables=0
candidate-calls-avoided=0". Neither experiment covers the observed scene.

Steady frame summaries in runtime.log (last 5 windows): 34.7, 35.5,
35.3, 35.6, 34.8 flips/s (mean 35.18). In runtime-previous.log
(last 5): 34.5, 36.4, 36.2, 35.8, 35.6 (mean 35.70).
Neither is a controlled independent A/B because the runs share
TLOU_COPY_BATCH=1. The user's observation of no improvement is
consistent with zero copy merges. Do not attribute a precise FPS delta
to this toggle.

Active-scene counters: ~24,000 upload batches and ~140 MiB per 5 s;
~18,000 mapped readbacks and ~2.5 MiB per 5 s, nearly all avoiding a
new GPU submission. TLOU transfers reports ~1.0–1.1 s of cumulative
GPU-tick waits per 5 s, versus ~0.06–0.07 s recording waits; note that
these are overlapping wall-time counters, not a GPU utilization profile.
The log also contains "Readback guard: kept 16 bytes ... game wrote after
the GPU"; bypassing producer waits, dirty checks or guard merging is
specifically unsafe.

Action: retire and remove v0.24 CopyBatch code, its launcher toggle
and test/header; force TLOU_SRT_DEP_BATCH=0 and remove its launcher
toggle. Preserve direct mapped readback and guard protections. The
source-only cleanup is not a new APK / improvement claim.

Next research gate: establish a workload-matched source of significant
*work elimination* in readback dependencies or draw processing, with
correctness proof and no speculative GPU/CPU memory reordering. PC
DrawReplay's measured 8–24% improvements cannot be transferred without
matching repeatability counters and resource-generation guards; TLOU's
indirect path currently lacks those gates. Do not ship an unvalidated
copy-batch variant simply because it compiles.
