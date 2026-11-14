// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>
#include "video_core/renderer_vulkan/tlou_compute_descriptor_reuse.h"

struct Buffer { std::uint64_t buffer, offset, range; };
using Reuse = Vulkan::Detail::ComputeDescriptorReuse<Buffer, std::uint64_t>;
int main() {
    Reuse cache;
    std::array<Buffer, 8> buffers{};
    for (unsigned i=0;i<buffers.size();++i)
        buffers[i]={100u+i, 1024u+i*256u, 512};
    const auto same = [&]() { return cache.Identical(123, 9, 0x1000, buffers, true); };
    assert(!same()); // first push required
    assert(same());  // identical handles, offsets and ranges: exact reuse
    ++buffers[3].offset; assert(!same()); assert(same());
    ++buffers[3].range; assert(!same()); assert(same());
    ++buffers[3].buffer; assert(!same()); assert(same());
    assert(!cache.Identical(123, 9, 0x2000, buffers, true)); // layout changed
    assert(!cache.Identical(124, 9, 0x2000, buffers, true)); // new command buffer
    assert(!cache.Identical(124, 10, 0x2000, buffers, true)); // direct mode touched
    assert(!cache.Identical(124, 10, 0x2000, buffers, false)); // direct mode
    assert(!cache.Identical(124, 10, 0x2000, buffers, true)); // fresh push
    assert(cache.Identical(124, 10, 0x2000, buffers, true));
    cache.Invalidate(); // fallback vkCmdPushDescriptorSetKHR took ownership
    assert(!cache.Identical(124, 10, 0x2000, buffers, true));
    assert(cache.Identical(124, 10, 0x2000, buffers, true));
    std::array<Buffer, 8> moved=buffers;
    moved[0]=moved[2];
    assert(!cache.Identical(124, 10, 0x2000, moved, true));
    assert(cache.Identical(124, 10, 0x2000, moved, true));
    std::uint64_t hits=0, misses=0;
    const auto start=std::chrono::steady_clock::now();
    for (std::uint32_t i=0;i<1000000;++i) {
        // Simulate 128 dispatches reusing a layout and four buffers, then a
        // new guest output range. Real Vulkan GPU readback is tested separately.
        if ((i & 127u)==0) ++buffers[0].offset;
        if (cache.Identical(150+i/16384, 11, 0x3000, buffers, true)) ++hits;
        else ++misses;
    }
    const auto end=std::chrono::steady_clock::now();
    assert(hits>980000 && misses>=7812);
    std::printf("TLOU descriptor reuse invariant PASS hits=%llu misses=%llu check=%.1fns/call\n",
        static_cast<unsigned long long>(hits),static_cast<unsigned long long>(misses),
        std::chrono::duration<double,std::nano>(end-start).count()/1000000);
}
