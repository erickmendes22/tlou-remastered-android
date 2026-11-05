# TLOU Android v0.23 — contiguous descriptor snapshots (CPU recording)

This change is isolated to Vulkan descriptor **recording**, not GPU readback.
This is an optional switch in the SAME APK: `TLOU_DESCRIPTOR_BULK=1` enables
bulk snapshots. Default OFF runs the original v0.21 per-write path.

v0.21 remains the control. Readback 2 remains removed; SRT/fine-tick experiments
remain reverted.

## Source-level reason

When threaded Vulkan recording is active and push descriptors are supported,
`Pipeline::BindResources` previously called `Scheduler::RecordData` for each
buffer/image descriptor write. `Rasterizer` produces these infos in two
contiguous, stable `static_vector` arrays. A draw with B buffer and I image
write pointers did roughly B+I separate info snapshots, even though the
arrays already existed contiguously.

The new path snapshots those two arrays **once each** and relocates each
`WriteDescriptorSet` pointer by its validated array offset. Every draw
still snapshots new descriptors, keeps the same binding order and descriptor
counts, pushes the same constants, and executes the same Vulkan call. No
descriptor is reused between draws, no host GPU wait is removed, and no
buffer/texture invalidation, readback fence, or guest memory write is skipped.
Pointers outside the arrays, including texel views, use the original
per-write snapshot fallback. All snapshots and their deferred command stay
in the same `RecordChunk`.

The expected improvement is fewer CPU-side snapshot calls/copies, **not a
proven FPS increase**. On devices without push descriptors or without
threaded recording, this path does not change the descriptor binding work.
The host C++ test covers contiguous offsets, arrays, bounds failures,
unrelated arrays, null pointers and misalignment.

## Validation gate

Native CI must compile and run before shipping. Compare this APK against the
existing v0.21 capture in the same heavy scene, with identical configuration.
Keep it only if stable median and 1% low FPS improve, and rendering stays
identical after save/load and repeated frames. All experiments are independently switchable in the launcher, default OFF.
This commit does **not** claim an on-device measurement.
