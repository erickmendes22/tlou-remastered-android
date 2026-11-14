// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

namespace Vulkan::Detail {
// The template reads exactly one descriptor-info object representation at
// each binding. Snapshot data afresh for every draw: NO handles, offsets,
// ranges or image layouts are reused from the previous draw.
template <class BufferInfo, class ImageInfo>
struct MixedDescriptorSlot {
    static constexpr std::size_t StorageSize =
        sizeof(BufferInfo) > sizeof(ImageInfo) ? sizeof(BufferInfo) : sizeof(ImageInfo);
    alignas(BufferInfo) alignas(ImageInfo) std::byte payload[StorageSize];
};

template <class Write, class DescriptorType, class Container>
bool PackMixedDescriptors(std::span<const Write> writes,
                          std::span<const DescriptorType> expected,
                          DescriptorType buffer_type, DescriptorType sampled_type,
                          DescriptorType sampler_type, Container& slots) {
    if (writes.size() != expected.size() || writes.size() < 6) return false;
    // The renderer supplies a stack-backed small_vector. No heap allocation
    // for normal push-descriptor layouts.
    slots.resize(writes.size());
    for (std::size_t i = 0; i < writes.size(); ++i) {
        const auto& w = writes[i];
        if (w.pNext || w.dstBinding != i || w.dstArrayElement != 0 ||
            w.descriptorCount != 1 || w.descriptorType != expected[i] ||
            w.pTexelBufferView) return false;
        auto& slot = slots[i];
        if (expected[i] == buffer_type) {
            if (!w.pBufferInfo || w.pImageInfo ||
                sizeof(*w.pBufferInfo) > sizeof(slot.payload)) return false;
            static_assert(std::is_trivially_copyable_v<std::remove_cv_t<
                std::remove_reference_t<decltype(*w.pBufferInfo)>>>);
            std::memcpy(slot.payload, w.pBufferInfo, sizeof(*w.pBufferInfo));
        } else if (expected[i] == sampled_type || expected[i] == sampler_type) {
            if (!w.pImageInfo || w.pBufferInfo ||
                sizeof(*w.pImageInfo) > sizeof(slot.payload)) return false;
            static_assert(std::is_trivially_copyable_v<std::remove_cv_t<
                std::remove_reference_t<decltype(*w.pImageInfo)>>>);
            std::memcpy(slot.payload, w.pImageInfo, sizeof(*w.pImageInfo));
        } else {
            return false; // storage images, arrays and unusual descriptors use old path
        }
    }
    return true;
}

template <class CommandBuffer, class Update, class Layout, class Slot>
void PushMixedDescriptorTemplate(CommandBuffer command, Update update, Layout layout,
                                 std::span<const Slot> slots) {
    // Vulkan-Hpp's const DataType& overload takes the ADDRESS OF A POINTER
    // if given a typed pointer. The raw void* overload is essential.
    command.pushDescriptorSetWithTemplateKHR(
        update, layout, 0, static_cast<const void*>(slots.data()));
}
} // namespace Vulkan::Detail
