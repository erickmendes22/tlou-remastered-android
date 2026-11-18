# TLOU Android 0.17.2 — A/B compute cache warm-up isolation

## New device logs, 2026-11-04

These logs are three independent **current** runs (not six: the *_previous
files repeat other ZIPs). Each uses 720p, Readback 1, Turnip Gen8 V37
patched, mapped readbacks ON, GPU profiler OFF, prep pool/CPU trims/
pipeline prefetch/parallel pipelines ON.

| Experiment | Steady flips/s, last 3 five-second windows |
| --- | ---: |
| split64 ON + mapped fault ON | (36.9 + 36.1 + 37.0)/3 = 36.67 |
| split64 OFF + mapped fault ON | (36.5 + 36.0 + 35.5)/3 = 36.00 |
| split64 ON + mapped fault OFF | (36.4 + 36.2 + 36.0)/3 = 36.20 |

The ~0.7 FPS observed difference is smaller than normal scene/test variability
and is **not a demonstrated speedup**. In the steady phase, there are about
24–25k transfer batches and ~140 MiB uploads per 5 seconds, and ~1,050–1,125 ms
of instrumented GPU tick waits per 5 seconds. These wall-clock waits overlap;
do not treat them as total GPU busy time.

Turning off mapped fault readbacks while leaving split64 ON did not improve
the steady-state FPS in this limited comparison. No change to mapped faults,
DMA batching, or synchronization was made.

## Verified cache problem and correction

The log still contains `its binary is not the one its description was
written with (hash differs)` for both compute shaders `22e6e19e` and
`61d67259`. Version 0.17.1 separated their metadata keys and binary
filenames by mode, but background warm-up **enumerates both modes**.
`LoadShaderMeta` did not check the stored permutation key against the current
mode, so it could load the opposite mode's metadata, select the *current*
mode's SPIR-V filename and wrongly treat the combination as corruption.
`BinaryMatchesMeta` then deleted valid cache entries and forced recompilation.

`LoadShaderMeta` now checks the stored permutation hash against the expected
hash for the active toggle **before** reading or deleting SPIR-V. Opposite-mode
entries are skipped without removal. The 0.17.1 keys remain unchanged, so
no full cache migration or reset is needed.

A unit regression checks both directions (1024->64 and 64->1024), both hashes,
non-target shader identities and the native indirect fallback. All new
experimental controls keep their existing defaults.
