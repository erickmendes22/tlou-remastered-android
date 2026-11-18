// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Capture the descriptor in the deferred command closure instead of an extra
// Scheduler::RecordData allocation. All GPU barriers/order stay unchanged.
#include <cstdint>
#include <span>
namespace Vulkan::InlineCopyRecord {
inline constexpr std::uint64_t MaxSize = 64 * 1024;
template <typename Region>
constexpr bool Eligible(std::span<const Region> regions) {
    return regions.size() == 1 && regions[0].size > 0 && regions[0].size <= MaxSize;
}
template <typename Buffer, typename Region>
auto Single(Buffer src, Buffer dst, Region region) {
    return [src, dst, region](auto cmdbuf) {
        cmdbuf.copyBuffer(src, dst, 1u, &region);
    };
}
} // namespace Vulkan::InlineCopyRecord
