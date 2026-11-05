# TLOU Android — Cross-port optimization evidence and rollback (2026-11-04)

## Why this review exists

Do **not** ship another APK based on an unvalidated theory about GPU waits.
TLOU Android's v0.21 and v0.22 heavy-scene runs in the user's ZIP have
different *sets* of enabled options, so their ~36.45 vs ~35.48 FPS
means **the v0.22 bundle regressed**, not that any individual option caused it.
The v0.22 fine-tick hypothesis specifically returned
`coarse-overwait-pages=0` during the heavy scene, so it was not a
credible explanation for the ~1.1 s / 5 s accumulated timeline waits.
The v0.21 SRT independent-table grouping had **zero groups** in stable
heavy-scene windows. Native tests prove correctness of helper functions,
not an in-game FPS win.

This commit restores the exact **v0.21 source tree** (including prior
working switches and Readback 1 only) instead of shipping another
uncontrolled hybrid of old and new options. The v0.21 APK was already
built/validated in workflow 41816380969. This rollback intentionally
uses [skip ci]: it is a source correction, not a new performance release.

## Sources with documented A/B gains; not Android claims

| Source and location | Reproducible evidence reported by the author | TLOU applicability and gate |
| --- | --- | --- |
| Lucas-blip-png/ShadowOfTheColossus-PC README, 0.60.6-plus3 | FAST_BIND ~+8%, 43.5 to 47.1 FPS (later fixed races, black dots) | TLOU BindResources is more complex with helper texture tasks/prepared stages. Missing SotC generation/epoch guards; measure read-only duplicate binds and design invalidation *before* shortcutting. |
| Same README, 0.60.6-plus4, buffer_cache.cpp GatherDownloads | SOTC_RB_HOT about +14% at the Shrine (no matched FPS pair in the cited release summary), with late checks and a 26-minute soak | Unprotected speculative readback previously corrupted guest data after save reload, even heap metadata. TLOU has separate lazy page writes/aliases and already ~18k direct mapped readbacks per 5 seconds; NEVER copy blindly. Requires CPU dirty checks, writer stamps, no speculative guest overwrites, and matched safety tests. |
| Same README, 0.60.6-plus13 and 14, rasterizer.cpp DrawReplay | +8.6% on Shrine and ~+24% desert in A/B, PGO adds ~6-7%, fast-path verify 0 mismatches in >50M checks | Investigate TLOU's draw recording and prepared-stage generation reuse. CPU command thread busy in user's logs. Do not port game-specific data keys or unguarded DMA state. |
| Same README, 0.60.6-plus5 | Storage-image barrier once per fence epoch: 53–54 to 59–61 FPS | TLOU already tracks barrier epochs and depth feedback. A corresponding Vulkan hazard/epoch proof is needed before eliding any barrier. |
| erickmendes22/tlou-remastered-android README and gow3_pc/build.sh (Windows) | TLOU PC same heavy scene ~+10% with PGO+LTO | Windows PGO profile cannot be used for ARM64 Bionic/FEX; investigate ARM native renderer LTO and generate ARM-specific profile before expecting a similar gain. |
| Lucas SotC README +26 | Worker naps +19% on four-thread CPUs | Snapdragon 8 Elite has more than four execution cores; cannot claim this benefit without CPU-affinity bottleneck evidence. |
| erickmendes22/gow3_android_daedalus_fork docs/ANDROID_PERFORMANCE_DELTA_025.md | Explicitly source analysis, not measured Android gain | Do not copy frames-ahead or GPU fences as though FPS benefit is proved. |
| Castlesea/infamous-second-son-pc-port README | 80-95 city FPS; native passes, no directly comparable before/after for TLOU | Game-specific native pass replacement is a long-term architecture project, not a safe one-commit optimization. |
| erickmendes22/uncharted-android and MaSieS4Fun/ARM_bloodborne_pc | Shared FEX/Bionic/Vulkan foundations; no matching, quantified TLOU heavy-scene readback A/B found | Reference source of synchronization/driver compatibility, not proof of a gain. |

## Existing TLOU protections to preserve

- Readback 2 (Precise) is **removed** from the Android launcher.
- `TLOU_MAPPED_READBACKS` is the baseline: direct coherent GPU mappings
  already avoid extra GPU submissions in nearly every mapped readback.
- `BufferCache::DownloadRanges` and `CopyMappedReadbackPieces` retain
  GPU producer semaphore waits, guest-byte merge, page ownership, and
  staging fallback. No copied speculative hot snapshots may overwrite
  a guest page that was written since its producer.
- `Rasterizer::BindResources` can execute texture binding on helpers
  while the main GPU thread binds buffers; SotC FastBind must consider
  these tasks, prepared stage generations, DMA, and sampled image
  residency before any descriptor re-use.
- The stable-scene logs show **zero shader compilation** and almost all
  mapped reads already avoid additional GPU submissions. These are
  not primary targets for a sustained heavy-scene FPS gain.

## Research gate before a new APK

1. Select one already documented cross-port optimization matching a
   measured TLOU hotspot. Prioritize draw resource binding and command
   recording; don't repeat ruled-out SRT/fine-tick hypotheses.
2. Build a source-level equivalence map including invalidation cases and
   demonstrate an optimization actually covers events in the TLOU
   heavy scene (static counts/instrumentation, not guessed).
3. Add correctness verification / fallback for reused descriptors and
   guest CPU/GPU writes; run native tests, CI, and visual regression checks.
4. Evaluate FPS (median, 1% low), command-thread time, GPU waits and
   correctness relative to the **already captured** baseline. Do not ask
   the user to repeat the baseline, nor claim Android benefit from PC
   data alone. Retire any experiment with zero coverage or no durable
   improvement; do not accumulate a menu of no-gain toggles.

References:
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC
- https://github.com/Castlesea/infamous-second-son-pc-port
- https://github.com/erickmendes22/tlou-remastered-android
- https://github.com/erickmendes22/gow3_android_daedalus_fork
- https://github.com/erickmendes22/uncharted-android
- https://github.com/MaSieS4Fun/ARM_bloodborne_pc

## Additional references checked and exclusions

- **GoW III Android changelog (measured):** the host-cached stream buffer
  optimization changed median FPS **34.6 -> 128.6 on NVIDIA**; the same
  changelog says **neutral on AMD UMA/Steam Deck**. The phone's Adreno
  shares system memory; do not transplant this setting using the NVIDIA
  result as evidence. The separate `IT_SET_PREDICATION` no-drain change
  is **already present** in TLOU's `liverpool.cpp`.
- **inFAMOUS Second Son `MeWeGo/infamous-pc` benchmark:** 30.6 median
  vs 16.6 in `iss_pc` in the same title scene, but its optimization is
  a *bundle* (uploads, bind/cache, worker, readbacks). It is not a
  single readback A/B. Its gameplay was not benchmarked in that table.
- **Bloodborne ARM port:** the published major speedup involves
  splitting command decode, draw recording, and other work across
  threads. TLOU logs already show independent busy command, draw and
  recorder threads; duplicating workers cannot be called equivalent.
- **SotC validation caution:** TLOU log 11 has explicit
  `Readback guard: kept 16 bytes ... game wrote after GPU`, proving
  this workload contains CPU writes that must survive a GPU download.
  A hot speculative readback must never replace that correctness rule.

## Quantified TLOU v0.21 / v0.22 comparison

- v0.21 last 4 stable windows: 37.3, 35.7, 36.6, 36.2
  = **36.45 FPS mean**.
- v0.22 last 4 stable windows: 35.7, 35.7, 35.0, 35.5
  = **35.475 FPS mean**; approximately **-2.7%**.
- v0.22 observed `coarse-overwait-pages=0` on the sampled scene;
  there is no demonstrated wait saving from the 64 KiB tick hypothesis.
- v0.22 log's CPU usage: GPU command ~76% of a core, draw
  ~65%, recorder ~50%. Shader compilation = 0 in steady gameplay.
- v0.21 had several additional toggles ON, so these numbers measure
  **whole configurations**, not the independent effect of fine ticks.
  An older build changed GPU allocations and readback paths together;
  FPS changes alone cannot identify the responsible toggle.
- For reference, GoW3's previously measured NVIDIA host-stream change
  is vendor-dependent; TLOU PC's PGO+LTO measurement is on Windows.
  Android's `android/bionic/build_gpu.sh` currently passes
  `-DGOW3_LTO=OFF -DGOW3_PGO=off`. Enabling LTO alone has no
  established Android FPS gain.

Direct references:
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/README.md
- https://github.com/AjncSystem/ShadowOfTheColossus-PC/blob/main/CHANGELOG.md
- https://github.com/erickmendes22/gow3_android_daedalus_fork/blob/android/CHANGELOG.md
- https://github.com/MeWeGo/infamous-pc#benchmarks
- https://github.com/MaSieS4Fun/ARM_bloodborne_pc#readme
- https://github.com/erickmendes22/tlou-remastered-android/blob/main/README.md
