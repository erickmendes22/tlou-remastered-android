# TLOU Android — eboot performance audit (2026-11-05)

**Scope:** user-supplied `eboot(20261105-002451).bin`, the two runs archived in `TLOU-Android-Logs (22)(2).zip`, current `android-bionic-stage1` and the public upstream PC/shadPS4 renderer. The binary is *not* committed or redistributed.

## 1. Binary identity and executable audit

- Original SELF size 24,718,840 bytes; SHA-256 `e5f0aa322f60bd90637cc10d83de08c77885b1921084021c630e04ed3d6febf7`.
- Embedded FreeBSD/PS4 ELF64 x86-64, entry `0x56d9f0`, matches `android/tlou_patches.py::BASE_SELF_SHA256` exactly.
- SELF executable payload offset `0xaff0` is ELF load at guest VA `0x0`. Static instruction checks used `SELF[0xaff0 + guest_va]`. ELF reconstruction was inspected with `llvm-objdump`; the reconstructed ELF was only a private analysis artifact.
- Original shader-effect patch signatures all match at `0x946363` (SSAO), `0x9463e2/0x946824` (motion blur), `0x9463fe` (DOF), `0x9464a4` (bloom), `0x8efd22` (sun shadows), and `0x8f2490` (subsurface).
- The runtime reported **46 patch writes**; exactly `7` executable base replacements + `32` 720p replacements + `7` replacements disabling all six graphical effects = **46**. With the current patch writer and resolution reported as 720, this strongly indicates all six effects were already disabled. The ZIP does not include the individual patch-write list, so this is a count inference, not direct readback of guest patched bytes.

## 2. Eboot synchronization/worker findings — already patched

- `0xa8febc` references the `NdJobWorkerThread` name. The runtime creates six numbered guest workers. The worker loop at `0xaaca36` checks sleep state at `[rdi+0x59]`; `0xaaca40` compares its retry counter to **1000**; `0xaaca49` calls the condition-variable wait; the submit-side check/broadcast is at `0xaabf91`.
- Current `BASE_PATCHES["job-sleep"]` already forces the sleep path and keeps submit notifications. Original instruction guards at `0xaaca3a`, `0xaaca40`, `0xaaca65`, `0xaabf91` matched the uploaded SELF. **Do not re-introduce job-sleep as a new optimisation.**
- Lowering its threshold from 1000 to e.g. 64 might reduce idle spin iterations, but worker CPU time and wake latency have *not* been measured in these logs. No threshold patch was introduced.
- The watchdog `WaitOnContextDone` at `0x99285a` already has the source timeout extension from 2 to 20 seconds. This avoids false timeouts but does not make the GPU finish early.

## 3. Per-frame CPU costs actually observed

Four final compilation-free windows, Adreno/Turnip Gen8 V37 patched, Readback 1, 720p, 34.6–36.5 flips/s:

| Metric | Mean |
| --- | ---: |
| Flips/s | 35.75 |
| Command-thread CPU time | **21.42 ms/frame** |
| Draw-thread CPU time | **18.07 ms/frame** |
| Recorder CPU time | **13.93 ms/frame** |
| Instrumented `gpu-ticks` wait | **5.92 ms/frame** |
| Host copy wall time | **0.65 ms/frame** |

Conversion: `thread_cpu_percent_of_one_core / 100 * interval_seconds * 1000 / (fps * interval_seconds)`. The threads overlap in time; these metrics must **not** be summed as independent end-to-end frame times. `gpu-ticks` are instrumented accumulated waits, not GPU execution duration.

The command thread's 21+ms CPU/frame and ~6ms/frame of waits are consistent with an approximately 28ms frame interval; the data justify investigating native renderer command decode, resource bind and queue synchronization, **not** assuming a specific guest thread or shader is still hot. Shader compiles were zero in these windows.

- 241 guest `DCLoader` thread *creation events* occurred in each run, concentrated in loading (176 in the first reporting interval, 57 in a later load). **Zero** DCLoader creation events occurred in the four final heavy-gameplay windows. This cannot explain steady-state low FPS, and does not prove 241 simultaneous live threads.
- Two previously profiled PS4 compute shader hashes `22e6e19e` and `61d67259` cost about 14.5ms combined in the **older v0.16 profiling run**. The new ZIP contains their 900/832-byte GCN binaries but not a new timestamp breakdown. Past split64/relaxed-math experiments did not demonstrate a sustained Android FPS gain; do not claim those shader costs as current.
- Direct mapped SRT readbacks and 720p+effect patches are already in the captured runtime. The temporal SRT prefetch published data but **did not improve the sustained FPS** in this capture; it was disabled by default in an earlier commit.

## 4. Source-level redundant operation removed — not an unmeasured FPS claim

The Android `TextureCache::FindImageFromRange` previously called `ForEachImageInRegion(address,size)` then discarded every candidate whose `image.info.guest_address != address`. That scans all 1 MiB pages crossed by the requested image size even though any image **starting** at `address` must be indexed in its **first page**. It can equivalently use `ForEachImageInRegion(address,1)`.

The source was changed to the first-page-only lookup, as in recent upstream shadPS4. Static page-index equivalence was also checked in 100,000 randomized positive-size cases. This is a real reduction in redundant address-index traversals with unchanged candidate acceptance rules, but its device coverage and FPS impact have **not** been measured. We deliberately did not trigger another APK build or claim a significant gain.

## 5. What is still missing for a justified high-impact change

- Attribute the **21.42ms/frame command CPU** to actual subphases (PM4 decode; resource table synchronization; `BindResources`; image lookup; `ObtainBuffer`; GPU wait). The current ZIP gives totals, not a per-function profile.
- Count equality/reuse of consecutive draw state, pipeline keys, descriptor generations and resource-ownership epochs before porting SotC FAST_BIND/DRAW_REPLAY. Its PC gains do not establish TLOU hit rate.
- Verify whether the two compute shaders are still on the current GPU critical path before altering their math or local workgroup size. Prior experiments failed to improve FPS.
- Keep previously proven readback corruption guards, Readback 1 semantics, graphics correctness and existing shader/effect patches. No new binary patch is justified by these captures.

**Decision:** No unverified eboot worker-count reduction, delay removal, forced frame-cap, speculative GPU readback or shader math rewrite; preserve current baseline and avoid another build without evidence of high coverage.
