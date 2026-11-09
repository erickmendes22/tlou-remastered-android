// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/buffer_cache/mapped_readback_plan.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <random>
#include <sys/mman.h>
#include <unistd.h>

struct Range { std::uint64_t address, size, offset; };
using namespace VideoCore;
constexpr std::uint64_t Base = 0x1000000000ull;

int main() {
    assert(!SmallMappedSnapshotEligible(0,false));
    assert(!SmallMappedSnapshotEligible(0,true));
    assert(!SmallMappedSnapshotEligible(512,false));
    assert(SmallMappedSnapshotEligible(1,true));
    assert(SmallMappedSnapshotEligible(512,true));
    assert(!SmallMappedSnapshotEligible(513,true));
    // A stack snapshot of the same size/offsets is already exercised below
    // by CopyMappedReadback(*plan, result, callback), where result is std::array.

    const auto page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const auto allocation = std::max<std::size_t>(8192,page_size*2);
    auto* source = static_cast<std::uint8_t*>(mmap(nullptr,allocation,PROT_NONE,
                                  MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(source != MAP_FAILED);
    std::array ranges{Range{Base+4080,48,8}};
    auto lookup = [&](std::uint64_t address) -> ReadbackMapping {
        if(address<Base || address>=Base+8192)return {};
        const auto offset=address-Base;
        return {source+offset,4096-offset%4096};
    };
    auto plan=PlanMappedReadback(std::span{ranges},96,10,lookup,
        [](std::uint64_t page){return page==(Base>>12) ? 2u:4u;});
    assert(plan && plan->pieces.size()==2 && plan->tick==4);
    std::array<std::uint8_t,96> result;result.fill(0xa5);
    bool waited=false;
    assert(CopyMappedReadback(*plan,result,[&](std::uint64_t tick){
        assert(tick==4);waited=true;
        assert(!mprotect(source,allocation,PROT_READ|PROT_WRITE));
        for(unsigned i=0;i<8192;++i)source[i]=static_cast<std::uint8_t>(i*17+3);
    })); // A read before the wait callback would fault on PROT_NONE.
    assert(waited);
    for(unsigned i=0;i<48;++i)assert(result[8+i]==source[4080+i]);
    assert(result[7]==0xa5 && result[56]==0xa5);

    auto missing=PlanMappedReadback(std::span{ranges},96,10,lookup,[](auto){return 0u;});
    assert(missing && missing->tick==10);
    unsigned current_waits=0;
    assert(CopyMappedReadback(*missing,result,[&](auto tick){assert(tick==10);++current_waits;}));
    assert(current_waits==1);
    assert(!PlanMappedReadback(std::span{ranges},96,10,lookup,[](auto){return 11u;}));
    assert(!PlanMappedReadback(std::span{ranges},96,0,lookup,[](auto){return 1u;}));
    assert(!PlanMappedReadback(std::span{ranges},MaxMappedReadbackBytes+1,10,lookup,[](auto){return 1u;}));
    auto gap=[&](std::uint64_t a)->ReadbackMapping{return a>=Base+4096 ? ReadbackMapping{}:lookup(a);};
    assert(!PlanMappedReadback(std::span{ranges},96,10,gap,[](auto){return 2u;}));
    bool invalid_wait=false;
    assert(!CopyMappedReadback(*plan,std::span{result}.first(32),[&](auto){invalid_wait=true;}));
    assert(!invalid_wait);
    std::array malformed{Range{~std::uint64_t{0}-4,16,0}};
    assert(!PlanMappedReadback(std::span{malformed},64,10,lookup,[](auto){return 2u;}));

    // Adjacent GPU virtual bytes can reside in distinct host allocations.
    std::array<std::uint8_t,4> first{1,2,3,4},second{5,6,7,8};
    std::array split_range{Range{Base+2,4,0}};
    const auto split=PlanMappedReadback(std::span{split_range},4,5,
        [&](std::uint64_t a)->ReadbackMapping {
            const auto off=a-Base;
            if(off<4)return {first.data()+off,4-off};
            if(off<8)return {second.data()+off-4,8-off};
            return {};
        },[](auto){return 2u;});
    std::array<std::uint8_t,4> joined{};
    assert(split && split->pieces.size()==2);
    assert(CopyMappedReadback(*split,joined,[](auto tick){assert(tick==2);}));
    assert((joined==std::array<std::uint8_t,4>{3,4,5,6}));

    // Fault readbacks keep a separate 1 MiB bound; the old SRT limit stays 64 KiB.
    constexpr std::size_t fault_bytes = 512 * 1024;
    auto* fault_source = static_cast<std::uint8_t*>(mmap(nullptr,fault_bytes,PROT_NONE,
        MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(fault_source != MAP_FAILED);
    std::array fault_ranges{Range{Base, 131072, 0}, Range{Base+196608, 262144, 131072}};
    auto fault_lookup = [&](std::uint64_t a)->ReadbackMapping {
        if(a<Base || a>=Base+fault_bytes)return {};
        const auto off=a-Base;
        return {fault_source+off,4096-off%4096};
    };
    const auto fault_plan=PlanMappedReadback(std::span{fault_ranges},393216,12,
        fault_lookup,[](auto){return 9u;},MaxMappedFaultReadbackBytes);
    assert(fault_plan && fault_plan->tick==9);
    assert(!PlanMappedReadback(std::span{fault_ranges},393216,12,fault_lookup,[](auto){return 9u;}));
    std::vector<std::uint8_t> fault_result(393216);
    unsigned fault_waits=0;
    assert(CopyMappedReadback(*fault_plan,fault_result,[&](auto tick){
        assert(tick==9);++fault_waits;
        assert(!mprotect(fault_source,fault_bytes,PROT_READ|PROT_WRITE));
        for(std::size_t i=0;i<fault_bytes;++i)fault_source[i]=static_cast<std::uint8_t>(i*13+7);
    }));
    assert(fault_waits==1);
    for(const auto& range:fault_ranges)
        assert(!std::memcmp(fault_result.data()+range.offset,fault_source+range.address-Base,range.size));
    assert(!PlanMappedReadback(std::span{fault_ranges},MaxMappedFaultReadbackBytes+1,12,
        fault_lookup,[](auto){return 9u;},MaxMappedFaultReadbackBytes));
    assert(!PlanMappedReadback(std::span{fault_ranges},393216,12,fault_lookup,
        [](auto){return 9u;},2*MaxMappedFaultReadbackBytes));
    assert(!munmap(fault_source,fault_bytes));

    std::vector<std::uint8_t> arena(65536),expected_gpu(65536);
    std::mt19937 rng(20);
    unsigned avoided_unrelated_submits=0;
    for(unsigned trial=0;trial<3000;++trial){
        for(auto& b:expected_gpu)b=static_cast<std::uint8_t>(rng());
        std::fill(arena.begin(),arena.end(),0);
        std::vector<Range> wanted;
        std::uint64_t bytes=0;
        for(unsigned i=0;i<1+trial%8;++i){
            const std::uint64_t count=1+rng()%2048,address=rng()%(65536-count);
            wanted.push_back({Base+address,count,bytes});
            bytes+=(count+63)&~std::uint64_t{63};
        }
        std::array<std::uint64_t,16> ticks;
        for(auto& t:ticks)t=1+rng()%8;
        const auto mapped=PlanMappedReadback(std::span{wanted},bytes,12,
            [&](std::uint64_t a)->ReadbackMapping{
                const auto offset=a-Base;
                if(offset>=arena.size())return {};
                return {arena.data()+offset,4096-offset%4096};
            },[&](std::uint64_t page){return ticks[page-(Base>>12)];});
        assert(mapped && mapped->tick<12);
        std::uint64_t required=0;
        for(const auto& r:wanted)
            for(auto p=r.address>>12;p<=(r.address+r.size-1)>>12;++p)
                required=std::max(required,ticks[p-(Base>>12)]);
        std::vector<std::uint8_t> direct(bytes,0x5a),staging(bytes,0x5a);
        assert(CopyMappedReadback(*mapped,direct,[&](std::uint64_t tick){
            assert(tick==required);
            arena=expected_gpu; // source becomes available only after its GPU completion
            ++avoided_unrelated_submits;
        }));
        for(const auto& r:wanted)
            std::copy_n(expected_gpu.data()+r.address-Base,r.size,staging.data()+r.offset);
        assert(direct==staging);
    }
    assert(avoided_unrelated_submits==3000);
    assert(!munmap(source,allocation));
    puts("mapped-readback: wait-before-read (guarded mapping), page/residency splits, missing/future ticks, gaps, bounds and 3000 staging-equivalent plans passed");
}
