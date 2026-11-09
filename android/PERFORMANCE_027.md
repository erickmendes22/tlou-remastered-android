# TLOU Android v0.27 — GPU compute-kernel arithmetic, not CP micro-optimizations

## Why this change instead of another CPU-side tweak

Source profile in Android v0.16 measured compute hashes `22e6e19e` and
`61d67259` at ~14.5 ms/frame combined in both 540p and 720p
under GPU timestamp profiling. This is not a measurement without
instrumentation and is NOT a projected gain. The binaries captured by
`GOW3_GPU_PROFILE=1` are 900 and 832 bytes, respectively, and are present
in the previously uploaded 12/13/16/17/18/19 ZIPs; the source
`android/PERFORMANCE_017.md` identifies them as vertex-transform kernels.
The GCN disassembly contains buffer loads/stores and dense vector arithmetic
(LLVM gfx803/gfx900 decoding of PS4-generation GCN is partial, so do
not infer precise shader semantics from those labels alone).

Previous attempts did NOT solve this:
- v0.17/17.1/17.2 split 1024 compute threads into 16x64; multiple A/B
  captures varied by <1 FPS. Keep split64 OFF by default; no new split.
- Direct mapped readbacks already avoid ~18k staging transfers per 5 s.
  Roughly 2.5 MiB/5 s of data is not a credible primary FPS lever.
  The same-heap `HOST_CACHED` memory type was found but is NOT changed:
  it may also penalize GPU bandwidth due CPU-cache snooping.
- v0.25 GENERAL barrier: zero barriers skipped; v0.26 NOP: ~1.8M
  per 5 s but no sustained FPS improvement. Both retired.
- The hot shader GPU time may be inflated by profiling; no gain can
  be assumed until actual device testing and visual validation.

## Source-level mechanism

`shader_recompiler/backend/spirv/emit_spirv_floating_point.cpp` unconditionally
emitted `spv::Decoration::NoContraction` for floating point operations.
The Vulkan spec allows arithmetic contraction/reassociation when the
decoration is absent; this gives the driver's SPIR-V optimizer a chance
to reduce FP work in a pair of historically GPU-expensive kernels.
The Arm GPU best practices guide also warns that this decoration can
prevent shader optimizations, but these findings are for Arm Mali:
**Adreno speedup is unverified.**

Opt-in `TLOU_RELAXED_SKINNING_FP=1`, OFF by default:
- Selects *only* compute-stage shader hash `22e6e19e` or `61d67259`.
- Skips the `NoContraction` decoration for FP32/FP64 op emissions
  in these two shader modules only. No GCN instructions, dispatch sizes,
  buffer layout, texture bindings, readbacks, or GPU sync are removed.
- All other graphics/compute shaders and OFF path are untouched.
- Floating point results can change from PS4 roundings (few ULPs,
  possibly more in sums); any geometry/skinning glitches invalidate
  the optimization. No blanket relaxed math for other shaders.
- The SPIR-V binary name gains `_tloufpv1` for the opt-in.
  Shader meta and Vulkan pipeline permutation hash use a separate
  `RelaxedFpVersionKey`. Four variants (native, split64, relaxed FP,
  both) are distinct. `LoadShaderMeta` rejects other-variant metadata
  before comparing binaries, preventing an old v0.17 corruption bug.
- Changing the option requires restarting the game.
- Launcher has one independent toggle under Experimentos.
  "Desativar todos" restores OFF.

## Validation/decision gate

Run `gow3_pc/tests/test_tlou_compute_split.cpp` with extended full
4x4 permutation/cache-key/metadata rejection checks.
Native ARM64 CI must build GPU and verify real APK, no PC-only compile.
For performance, compare the already saved baseline with the same
app and settings except relaxed-FP OFF/ON; **do not ask for an additional
OFF log that already exists**. Monitor scene geometry, animation,
reflections and gameplay before deciding a benefit.
Accept ONLY if a persistent, material FPS or GPU-time improvement is
measured. Remove if none, or if numerical/graphic regressions occur.
No additional build for microtrials or memory type selection.

Reference:
- `android/PERFORMANCE_016.md`, `android/PERFORMANCE_017*.md`
- https://docs.vulkan.org/spec/latest/appendices/spirvenv.html
- https://documentation-service.arm.com/static/67a62b17091bfc3e0a947695
