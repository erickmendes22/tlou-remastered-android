// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// TLOU: batch only independent, already resolved SRT table readbacks.
// The actual GPU tracking, synchronization and guest-memory writes remain
// owned by BufferCache::DownloadRanges. This helper only decides when a
// pending group must be flushed before resolving a child pointer.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Vulkan::TlouSrtBatch {
inline constexpr std::size_t MaxRanges = 8;
inline constexpr std::uint64_t MaxBytes = 64 * 1024;

template <typename Index>
bool ParentPending(int parent, std::span<const Index> pending_indices) {
    if (parent < 0) return false;
    return std::find(pending_indices.begin(), pending_indices.end(),
                     static_cast<Index>(parent)) != pending_indices.end();
}

constexpr bool Fits(std::size_t pending_count, std::uint64_t pending_bytes,
                    std::uint64_t next_bytes) {
    return next_bytes != 0 && next_bytes <= MaxBytes &&
           pending_count < MaxRanges && pending_bytes <= MaxBytes - next_bytes;
}
} // namespace Vulkan::TlouSrtBatch
