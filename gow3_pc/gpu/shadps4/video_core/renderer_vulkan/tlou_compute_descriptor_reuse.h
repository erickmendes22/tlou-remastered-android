// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace Vulkan::Detail {
// The caller must invalidate on EVERY other compute descriptor write and on
// direct command-buffer access. The epoch changes on every submission.
// This only deduplicates exact Vulkan descriptor state; dispatches, push
// constants and memory/barrier commands still execute in their original order.
template <class BufferInfo, class Layout>
class ComputeDescriptorReuse {
public:
    void Invalidate() noexcept {
        valid = false;
    }
    [[nodiscard]] bool Identical(std::uint64_t tick, std::uint64_t direct_epoch,
                                 Layout layout, std::span<const BufferInfo> current,
                                 bool recording_deferred) {
        if (!recording_deferred) {
            Invalidate();
            return false;
        }
        bool same = valid && saved_tick == tick && saved_direct_epoch == direct_epoch &&
                    saved_layout == layout && saved.size() == current.size();
        if (same) {
            for (std::size_t i = 0; i < current.size(); ++i) {
                const auto& a = saved[i];
                const auto& b = current[i];
                if (a.buffer != b.buffer || a.offset != b.offset || a.range != b.range) {
                    same = false;
                    break;
                }
            }
        }
        if (same) return true;
        saved_tick = tick;
        saved_direct_epoch = direct_epoch;
        saved_layout = layout;
        saved.assign(current.begin(), current.end());
        valid = true;
        return false;
    }
private:
    bool valid = false;
    std::uint64_t saved_tick{};
    std::uint64_t saved_direct_epoch{};
    Layout saved_layout{};
    std::vector<BufferInfo> saved;
};
} // namespace Vulkan::Detail
