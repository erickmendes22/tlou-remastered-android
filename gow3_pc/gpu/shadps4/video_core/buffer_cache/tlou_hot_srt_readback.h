// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
// Hot SRT readbacks may only piggyback on an already-needed GPU wait,
// never submit future work or wait for a later producer.
// The caller additionally enforces mapped residency and CPU-dirty/lazy-page guards.
namespace TlouHotSrtReadback {
constexpr bool Eligible(std::uint64_t candidate_tick, std::uint64_t primary_tick,
                        std::uint64_t current_tick, std::uint64_t candidate_size,
                        std::uint64_t remaining_budget) noexcept {
    return candidate_tick != 0 && primary_tick != 0 &&
           candidate_tick <= primary_tick && primary_tick < current_tick &&
           candidate_size != 0 && candidate_size <= remaining_budget;
}
// The late check must happen after waiting, immediately before guest publication.
constexpr bool PublishAllowed(bool cpu_modified, bool pending_readback,
                              bool lazy_page, bool entire_range_still_gpu_owned) noexcept {
    return !cpu_modified && !pending_readback && !lazy_page && entire_range_still_gpu_owned;
}
constexpr bool PagesDisjoint(std::uint64_t a, std::uint64_t asize,
                             std::uint64_t b, std::uint64_t bsize) noexcept {
    if (!asize || !bsize) return true;
    return ((a + asize - 1) >> 12) < (b >> 12) ||
           ((b + bsize - 1) >> 12) < (a >> 12);
}
constexpr bool Overlaps(std::uint64_t a, std::uint64_t asize,
                        std::uint64_t b, std::uint64_t bsize) noexcept {
    return a < b + bsize && b < a + asize;
}
} // namespace TlouHotSrtReadback
