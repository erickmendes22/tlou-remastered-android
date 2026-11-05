// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/renderer_vulkan/vk_inline_copy_record.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>
struct Region { uint64_t srcOffset, dstOffset, size; };
struct Result { uint64_t src{}, dst{}; Region copy{}; unsigned count{}; };
struct Cmd {
    Result* result;
    void copyBuffer(uint64_t src, uint64_t dst, uint32_t count, const Region* region) {
        result->src=src;result->dst=dst;result->count=count;result->copy=*region;
    }
};
int main() {
    using namespace Vulkan::InlineCopyRecord;
    assert(!Eligible(std::span<const Region>{}));
    std::array<Region,2> regs{{{13,29,MaxSize},{31,47,5}}};
    assert(Eligible(std::span<const Region>(regs.data(),1)));
    assert(!Eligible(std::span<const Region>(regs.data(),2)));
    regs[0].size=MaxSize+1;
    assert(!Eligible(std::span<const Region>(regs.data(),1)));
    regs[0].size=0;
    assert(!Eligible(std::span<const Region>(regs.data(),1)));
    Result result{};
    std::function<void(Cmd)> pending;
    {
        std::vector<Region> temp{{13,29,4096}};
        pending=Single(uint64_t(0x123),uint64_t(0x456),temp.front());
        temp.front()={1,2,999};
    }
    pending(Cmd{&result});
    assert(result.src==0x123 && result.dst==0x456 && result.count==1);
    assert(result.copy.srcOffset==13 && result.copy.dstOffset==29 && result.copy.size==4096);
    std::puts("TLOU inline deferred small copy: PASS");
}
