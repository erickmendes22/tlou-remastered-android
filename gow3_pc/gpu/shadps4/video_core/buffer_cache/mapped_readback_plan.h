// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace VideoCore {
inline constexpr std::uint64_t MaxMappedReadbackBytes = 64 * 1024;
inline constexpr std::uint64_t MaxMappedFaultReadbackBytes = 1024 * 1024;
inline constexpr std::uint64_t MaxStackMappedReadbackBytes = 512;
// Exactly the same planned readback; only scratch allocation changes.
constexpr bool SmallMappedSnapshotEligible(std::uint64_t bytes, bool enabled) {
    return enabled && bytes > 0 && bytes <= MaxStackMappedReadbackBytes;
}
struct ReadbackMapping { const std::uint8_t* data{}; std::uint64_t size{}; };
struct MappedReadbackPiece {
    const std::uint8_t* data;
    std::uint64_t offset, size;
};
struct MappedReadbackPlan {
    std::vector<MappedReadbackPiece> pieces;
    std::uint64_t bytes{}, tick{};
};

// Range has address/size/offset fields. Missing write history means wait for the
// current submission, never assume the data is complete. Mapping lookup must
// supply only coherent host-visible memory. Call without cache locks held at wait.
template <typename Range, std::size_t Extent, typename Lookup, typename PageTick>
std::optional<MappedReadbackPlan> PlanMappedReadback(std::span<Range, Extent> ranges,
        std::uint64_t bytes, std::uint64_t current_tick, Lookup&& lookup, PageTick&& page_tick,
        std::uint64_t max_bytes = MaxMappedReadbackBytes) {
    if (!bytes || bytes > max_bytes || max_bytes > MaxMappedFaultReadbackBytes || !current_tick) return std::nullopt;
    MappedReadbackPlan plan;
    plan.bytes = bytes;
    for (const auto& range : ranges) {
        if (range.offset > bytes || range.size > bytes - range.offset ||
            range.address > std::numeric_limits<std::uint64_t>::max() - range.size)
            return std::nullopt;
        std::uint64_t cursor = range.address;
        const std::uint64_t end = cursor + range.size;
        while (cursor < end) {
            const ReadbackMapping mapping = lookup(cursor);
            if (!mapping.data || !mapping.size) return std::nullopt;
            const auto count = std::min(mapping.size, end - cursor);
            plan.pieces.push_back({mapping.data, range.offset + cursor - range.address, count});
            cursor += count;
        }
        if (!range.size) continue;
        for (auto page = range.address >> 12; page <= (end - 1) >> 12; ++page) {
            const std::uint64_t tick = page_tick(page);
            if (tick > current_tick) return std::nullopt;
            plan.tick = std::max(plan.tick, tick ? tick : current_tick);
        }
    }
    if (plan.pieces.empty()) return std::nullopt;
    return plan;
}

template <typename Wait>
bool CopyMappedReadback(const MappedReadbackPlan& plan, std::span<std::uint8_t> snapshot, Wait&& wait) {
    if (snapshot.size() < plan.bytes || !plan.tick) return false;
    for (const auto& p : plan.pieces)
        if (!p.data || p.offset > snapshot.size() || p.size > snapshot.size() - p.offset) return false;
    wait(plan.tick); // includes submitting unsubmitted writes, when required
    for (const auto& p : plan.pieces)
        std::memcpy(snapshot.data() + p.offset, p.data, static_cast<std::size_t>(p.size));
    return true;
}
} // namespace VideoCore
