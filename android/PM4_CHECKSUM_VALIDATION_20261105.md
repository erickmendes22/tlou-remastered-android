# PM4 checksum: command-thread cost candidate, 2026-11-05

## Scope and why this is on a real path

`Liverpool::ApplyGraphicsRegisterPacket` calls `HashRegisterPacket` after
each graphics-register update. The draw-preparation scanner and workers
call the same function. This is work in command decoding, rather than an
assumption of cross-draw descriptor reuse. The rolling identity authorizes
speculative prepared draws only when the GPU thread and worker replay agree.

The previous scalar hash is a dependency chain of multiply/XOR/shift per
32-bit word. The candidate preserves that exact path for up to 7 words and
uses the runtime's existing `XXH3_64bits_withSeed` dependency for larger
packets. Header, payload, length and previous rolling identity all contribute.
`HashRegisterPacket` is still the single entry point for every replay path.
The identity is process-local; no persisted shader or pipeline cache is
changed. No readback order, memory ownership, shader or rendering state is
altered. As before, a 64-bit identity is probabilistic, not a proof of byte
equality; the change does not claim collision impossibility.

## Work done before selecting this candidate

Same-page SRT prefetch was rejected: fewer checks did not shorten modeled
producer waits and the host harness did not show CPU savings.

Two further queue candidates were measured and not integrated:

- Combining command execution and destruction into one dispatch reduced
  isolated queue cost but failed to show stable throughput gains with
  producer/consumer threads and prefetch enabled.
- Combining push constants and push descriptors halved queue entries in
  that operation and saved 32 bytes per bind in the synthetic shape, but
  end-to-end host results varied by descriptor count, including regressions.

Neither queue candidate is in this production change. Their isolated
percentages must not be reported as TLOU FPS gains.

## Measurements and limits

The standalone test includes the exact production checksum helper, links
XXHash externally like the runtime, and benchmarks chained dependent
packets. It uses warmup and 9 alternating-order samples of 200,000 packets
per size. Inputs are random but reproducible. This measures the function's
CPU cost, **not** the complete PM4 decoder, renderer or game frame.

An x86-64 host run with XXHash 0.8.2 gave the following median costs before
the small-packet cutoff was raised from 4 to 7 words:

| Words | Original ns/packet | XXH3 candidate ns/packet |
| --- | ---: | ---: |
| 5 | 8.56 | 8.63 |
| 8 | 13.80 | 8.50 |
| 16 | 27.33 | 10.17 |
| 32 | 54.73 | 12.82 |
| 64 | 109.60 | 38.79 |
| 256 | 437.56 | 70.52 |
| 1024 | 1923.87 | 247.42 |

The 5-word case failed to improve in this run; packets <=7 words therefore
keep the scalar path.

Native ARM64 and x86-64 validation completed successfully in workflow
`42008243896`, commit `2653447ad74b90a376c03888fd3f494dccb97bd4`.
Both architectures passed the correctness and ASan/UBSan checks. ARM64
used XXHash 0.8.2 and produced closely matching independent repeats:

| Words | ARM64 original ns | ARM64 candidate ns | Repeat candidate ns |
| --- | ---: | ---: | ---: |
| 3 | 3.60 | 3.58 | 3.58 |
| 7 | 8.35 | 8.35 | 8.33 |
| 8 | 9.50 | 5.64 | 5.64 |
| 16 | 18.94 | 8.34 | 8.34 |
| 32 | 37.77 | 14.31 | 14.34 |
| 64 | 75.49 | 26.24 | 26.30 |
| 256 | 301.83 | 56.15 | 56.17 |
| 1024 | 1207.41 | 199.03 | 198.83 |

This passes the primitive-cost gate on native ARM64. Integration uses a
`[native-only]` commit: the existing Bionic workflow compiles and links
the runtime and verifies its ARM64 symbols, but skips APK assembly,
signing and publication. The integration check itself is not a device
gameplay or FPS test.

The supplied log does not contain a complete register-packet length
histogram or a per-function profile. It cannot establish the fraction of
command-thread CPU occupied by this hash. A percentage improvement here
must **not** be converted into an FPS estimate or claimed as a significant
end-to-end improvement. It establishes a repeatable reduction of one real
primitive only if confirmed on the native ARM64 runner too.

## Correctness and validation-only workflow

`test_pm4_checksum_cost.cpp` checks:

- Exact legacy equivalence for short packets, zero-length identity and
  deterministic identical replay.
- Changed words, changed rolling seeds and packet ordering in 100,000
  reproducible random cases (not a collision proof).
- Every input length ending at an inaccessible page boundary, to detect
  accidental reads beyond the requested packet.
- AddressSanitizer and UndefinedBehaviorSanitizer.

`TLOU PM4 checksum correctness and CPU cost` runs on x86-64 and native
ARM64 GitHub hosts. It produces benchmark text only, **no APK**. The
native ARM64 runner is not a Snapdragon 8 Elite; device FPS, workload
coverage and graphics correctness still require later validation.
