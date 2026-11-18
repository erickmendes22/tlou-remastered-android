// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// TLOU v0.17: exact-hash, opt-in repartition of an independent compute kernel.
// A virtual 1024x1x1 group maps to sixteen physical 64x1x1 groups.
#include <array>
#include <cstdint>
#include <cstdlib>

namespace TlouComputeSplit {
inline constexpr std::uint64_t HashA = 0x22e6e19eULL;
inline constexpr std::uint64_t HashB = 0x61d67259ULL;
inline constexpr std::uint32_t OriginalSize = 1024;
inline constexpr std::uint32_t PhysicalSize = 64;
inline constexpr std::uint32_t Factor = OriginalSize / PhysicalSize;

constexpr bool IsTarget(std::uint64_t hash) { return hash == HashA || hash == HashB; }
// The 64-thread SPIR-V has a separate *pipeline and metadata key*, not only
// a different binary filename. Otherwise alternating OFF/ON overwrites
// shader metadata and forces recompilation when returning to the other mode.
inline constexpr std::uint64_t SplitVersionKey = 0xe4c8e23263da959bULL;
constexpr std::uint64_t VariantPermutationKey(std::uint64_t shader_hash,
                                               std::uint64_t original_key,
                                               bool split_enabled) {
    return split_enabled && IsTarget(shader_hash) ? original_key ^ SplitVersionKey
                                                  : original_key;
}
// Warmup scans pipeline keys from both variants. Metadata for the other
// variant is not corrupt: reject it before BinaryMatchesMeta can erase
// the current variant's valid SPIR-V and metadata.
constexpr bool AcceptCachedMeta(std::uint64_t shader_hash, std::uint64_t original_key,
                                std::uint64_t stored_key, bool split_enabled) {
    return !IsTarget(shader_hash) ||
           VariantPermutationKey(shader_hash, original_key, split_enabled) == stored_key;
}
inline bool Enabled() {
    static const bool enabled = [] {
        const char* env = std::getenv("TLOU_SPLIT_CS64");
        return env && env[0] == '1' && env[1] == 0;
    }();
    return enabled;
}
inline std::uint64_t PermutationKey(std::uint64_t shader_hash,
                                    std::uint64_t original_key) {
    return VariantPermutationKey(shader_hash, original_key, Enabled());
}
constexpr bool EligibleGeometry(std::uint64_t hash, std::array<std::uint32_t, 3> local,
                                std::uint32_t shared_bytes, bool unsafe_group_ops) {
    return IsTarget(hash) && local[0] == OriginalSize && local[1] == 1 &&
           local[2] == 1 && shared_bytes == 0 && !unsafe_group_ops;
}
inline bool Use(std::uint64_t hash, std::array<std::uint32_t, 3> local,
                std::uint32_t shared_bytes, bool unsafe_group_ops) {
    return Enabled() && EligibleGeometry(hash, local, shared_bytes, unsafe_group_ops);
}
// Keep original LocalSize for indirect dispatches and for oversized direct dispatches;
// they cannot share the optimized shader's program-cache identity.
constexpr std::uint64_t NativeFallbackHash(std::uint64_t hash) {
    return hash | (std::uint64_t{1} << 63);
}
constexpr bool CanExpandX(std::uint32_t groups, std::uint32_t limit) {
    return groups > 0 && groups <= limit / Factor;
}
constexpr std::uint32_t PhysicalGroupsX(std::uint32_t groups) { return groups * Factor; }
constexpr std::uint32_t VirtualGroupX(std::uint32_t group) { return group / Factor; }
constexpr std::uint32_t VirtualLocalX(std::uint32_t group, std::uint32_t local) {
    return (group % Factor) * PhysicalSize + local;
}
} // namespace TlouComputeSplit
