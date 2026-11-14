// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include "video_core/renderer_vulkan/tlou_mixed_descriptor_template.h"

struct Buffer { std::uint64_t buffer, offset, range; };
struct Image { std::uint64_t sampler, image; std::uint32_t layout, padding; };
enum class Type { Buffer, Sampled, Sampler, StorageImage };
struct Write {
    void* pNext{};
    std::uint32_t dstBinding{}, dstArrayElement{}, descriptorCount{1};
    Type descriptorType{};
    const Buffer* pBufferInfo{};
    const Image* pImageInfo{};
    const void* pTexelBufferView{};
};
using Slot = Vulkan::Detail::MixedDescriptorSlot<Buffer, Image>;
static_assert(sizeof(Slot) == sizeof(Buffer));
static_assert(alignof(Slot) == alignof(Buffer));

int main() {
    for (unsigned count : {4u, 6u, 8u, 16u, 24u}) {
        std::vector<Type> types(count);
        std::vector<Write> writes(count);
        std::vector<Buffer> buffers(count);
        std::vector<Image> images(count);
        std::vector<Slot> slots;
        slots.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            types[i] = i % 3 == 0 ? Type::Buffer :
                       (i % 3 == 1 ? Type::Sampled : Type::Sampler);
            writes[i].dstBinding = i;
            writes[i].descriptorType = types[i];
            if (types[i] == Type::Buffer) writes[i].pBufferInfo = &buffers[i];
            else writes[i].pImageInfo = &images[i];
        }
        const auto pack = [&]() {
            return Vulkan::Detail::PackMixedDescriptors(
                std::span<const Write>{writes}, std::span<const Type>{types},
                Type::Buffer, Type::Sampled, Type::Sampler, slots);
        };
        assert(pack() == (count >= 6));
        if (count < 6) continue;
        for (unsigned round = 0; round < 1000; ++round) {
            for (unsigned i = 0; i < count; ++i) {
                buffers[i] = {1000 + i, 256 * round + i, 500 + i};
                images[i] = {2000 + i, 3000 + i, (round + i) & 7u, 0};
            }
            assert(pack());
            for (unsigned i = 0; i < count; ++i) {
                const void* source = types[i] == Type::Buffer
                    ? static_cast<const void*>(&buffers[i])
                    : static_cast<const void*>(&images[i]);
                assert(std::memcmp(slots[i].payload, source, sizeof(Slot)) == 0);
            }
            // Mutate the original after packing; snapshot must not change.
            auto saved = slots[0];
            buffers[0].offset += 7;
            assert(std::memcmp(slots[0].payload, saved.payload, sizeof(Slot)) == 0);
        }
        writes[1].pNext = &writes[0]; assert(!pack()); writes[1].pNext = nullptr;
        writes[1].dstArrayElement = 1; assert(!pack()); writes[1].dstArrayElement = 0;
        writes[1].descriptorCount = 2; assert(!pack()); writes[1].descriptorCount = 1;
        writes[1].dstBinding = 99; assert(!pack()); writes[1].dstBinding = 1;
        writes[1].descriptorType = Type::StorageImage; assert(!pack());
        writes[1].descriptorType = types[1];
        writes[1].pImageInfo = nullptr; assert(!pack()); writes[1].pImageInfo = &images[1];
        writes[1].pTexelBufferView = &writes[1]; assert(!pack());
        writes[1].pTexelBufferView = nullptr;
        assert(pack());
    }
}
