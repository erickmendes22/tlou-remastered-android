// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic harness, NOT a Vulkan/device benchmark. Uses the production mapped
// planner, copier and publication gate. Dirty ownership and timeline progress
// are modeled explicitly; no sleeps or invented per-wait CPU cost are used.
#include "../gpu/shadps4/video_core/buffer_cache/mapped_readback_plan.h"
#include "../gpu/shadps4/video_core/buffer_cache/tlou_hot_srt_readback.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <numeric>

using namespace VideoCore;
struct Request { std::uint64_t address, size; };
struct Piece { std::uint64_t address, size, offset; std::size_t cell; };
struct Stats {
    std::uint64_t calls{}, blocking{}, modeled_stall{}, copied{}, skipped{};
};
struct Outcome { Stats stats; std::uint64_t checksum{}; };
enum class Scenario { Shared, NoLocality, Rewrite, CpuWrite, Unused };

Outcome Replay(std::span<const Request> trace, bool ahead, Scenario scenario) {
    struct Cell {
        Request request;
        std::array<std::uint8_t,128> gpu{}, guest{};
        bool dirty = true;
    };
    std::vector<Cell> cells;
    for (const auto& r : trace) {
        if (std::ranges::any_of(cells,[&](const auto& c){return c.request.address==r.address;}))
            continue;
        assert(r.size && r.size<=128 && (r.address&4095)+r.size<=4096);
        Cell c{};c.request=r;
        for (std::size_t b=0;b<r.size;++b)c.gpu[b]=static_cast<std::uint8_t>(r.address+b*19+7);
        cells.push_back(c);
    }
    if (scenario==Scenario::Unused) {
        Cell c{};c.request={trace[0].address+256,64};c.gpu.fill(42);
        cells.push_back(c); // GPU-written bytes on this page, never requested.
    }
    Stats stats;
    std::uint64_t now=0, producer=10;
    std::vector<bool> seen(cells.size());
    for (std::size_t event=0;event<trace.size();++event) {
        const auto r=trace[event];
        const auto it=std::ranges::find_if(cells,[&](const auto& c){return c.request.address==r.address;});
        assert(it!=cells.end());
        const std::size_t primary=it-cells.begin();
        // A repeated logged sync of the exact same address requires another
        // write; the log has no producer schedule. Never treat it as a free hit.
        if (seen[primary] || scenario==Scenario::Rewrite) {
            it->dirty=true; ++producer;
            for(std::size_t b=0;b<r.size;++b)it->gpu[b]^=0x37;
        }
        seen[primary]=true;
        if (!it->dirty) {
            assert(std::equal(it->guest.begin(),it->guest.begin()+r.size,it->gpu.begin()));
            continue;
        }
        std::vector<Piece> pieces{{r.address,r.size,0,primary}};
        std::uint64_t bytes=(r.size+63)&~std::uint64_t{63};
        if (ahead) {
            // A deliberately favorable model of the production discovery step:
            // all initial logged fields are already dirty, resident and eligible.
            // This is an upper bound, NOT established by the device log.
            for(std::size_t j=0;j<cells.size() && pieces.size()<17;++j) {
                const auto& c=cells[j];
                if(j==primary || !c.dirty || (c.request.address>>12)!=(r.address>>12))continue;
                const auto aligned=(c.request.size+63)&~std::uint64_t{63};
                if(bytes+aligned>4096)break;
                pieces.push_back({c.request.address,c.request.size,bytes,j});
                bytes+=aligned;
            }
        }
        auto lookup=[&](std::uint64_t a)->ReadbackMapping {
            for(const auto& c:cells)
                if(a>=c.request.address && a<c.request.address+c.request.size) {
                    auto off=a-c.request.address;
                    return {c.gpu.data()+off,c.request.size-off};
                }
            return {};
        };
        // Same page => same page/window producer lookup in production.
        const auto plan=PlanMappedReadback(std::span{pieces},bytes,producer+1,lookup,
                                           [&](auto){return producer;});
        assert(plan);
        std::vector<std::uint8_t> snapshot(bytes);
        bool late_cpu=false;
        assert(CopyMappedReadback(*plan,snapshot,[&](auto tick){
            ++stats.calls;
            const auto ready=tick*100; // synthetic timeline units, NOT nanoseconds
            if(now<ready){++stats.blocking;stats.modeled_stall+=ready-now;now=ready;}
            if(scenario==Scenario::CpuWrite && pieces.size()>1) {
                late_cpu=true;
                cells[pieces[1].cell].guest[0]=0xa5;
            }
        }));
        for(std::size_t p=0;p<pieces.size();++p) {
            const auto& piece=pieces[p];auto& c=cells[piece.cell];
            stats.copied+=piece.size;
            if(p && !TlouHotSrtReadback::PublishAllowed(late_cpu,false,false,c.dirty)) {
                ++stats.skipped; assert(c.dirty);continue;
            }
            std::copy_n(snapshot.data()+piece.offset,piece.size,c.guest.data());c.dirty=false;
        }
        assert(std::equal(it->guest.begin(),it->guest.begin()+r.size,it->gpu.begin()));
        if(late_cpu)assert(cells[pieces[1].cell].guest[0]==0xa5);
    }
    std::uint64_t checksum=0;
    for(std::size_t i=0;i<cells.size();++i)if(seen[i])
        checksum+=std::accumulate(cells[i].guest.begin(),cells[i].guest.end(),std::uint64_t{});
    return {stats,checksum};
}

// Exact first 16 SRT sync addresses/sizes from tlou/runtime.log in
// TLOU-Android-Logs (23).zip, SHA256
// 3892e329029b26bf34c6f40f1ec1459943b814ec2318ce916c991d05aaa51106.
constexpr std::array Log23{
    Request{0x11446bd900,24},Request{0x11446c32c0,64},Request{0x11446c5a80,56},
    Request{0x11446a4d00,92},Request{0x11446a5300,48},Request{0x11446a2940,64},
    Request{0x11446a3a00,64},Request{0x114469cf40,112},Request{0x11446a0c40,112},
    Request{0x114469f2c0,112},Request{0x11446c3f80,16},Request{0x114469a3c0,112},
    Request{0x1144698a40,112},Request{0x11446966c0,112},Request{0x114469c0c0,64},
    Request{0x11446a3a00,64}};

void Compare(const char* name, std::span<const Request> trace, Scenario scenario,
             bool expect_reduction) {
    const auto base=Replay(trace,false,scenario), opt=Replay(trace,true,scenario);
    assert(base.checksum==opt.checksum);
    assert(opt.stats.modeled_stall==base.stats.modeled_stall);
    assert(opt.stats.blocking==base.stats.blocking);
    assert(expect_reduction ? opt.stats.calls<base.stats.calls : opt.stats.calls==base.stats.calls);
    if(scenario==Scenario::CpuWrite)assert(opt.stats.skipped>0);
    if(scenario==Scenario::Rewrite || scenario==Scenario::CpuWrite || scenario==Scenario::Unused)
        assert(opt.stats.copied>base.stats.copied);
    std::printf("%s: calls=%llu/%llu blocking=%llu/%llu modeled-stall=%llu/%llu "
                "copied-bytes=%llu/%llu (baseline/ahead)\n",name,
        (unsigned long long)base.stats.calls,(unsigned long long)opt.stats.calls,
        (unsigned long long)base.stats.blocking,(unsigned long long)opt.stats.blocking,
        (unsigned long long)base.stats.modeled_stall,(unsigned long long)opt.stats.modeled_stall,
        (unsigned long long)base.stats.copied,(unsigned long long)opt.stats.copied);
}
int main() {
    constexpr std::array pair{Request{0x11446c32c0,64},Request{0x11446c3f80,16}};
    constexpr std::array separate{Request{0x1000,64},Request{0x2000,64},Request{0x3000,64}};
    Compare("same-page",pair,Scenario::Shared,true);
    Compare("log23-optimistic-ownership",Log23,Scenario::Shared,true);
    Compare("no-locality",separate,Scenario::NoLocality,false);
    Compare("GPU-rewrites-before-use",pair,Scenario::Rewrite,false);
    Compare("CPU-write-during-wait",pair,Scenario::CpuWrite,false);
    Compare("unused-dirty-field",std::span{pair}.first(1),Scenario::Unused,false);
    // Alternating order, warmup, 9 samples. Wall CPU cost is informational:
    // includes harness state, NOT full BufferCache/Vulkan/Adreno execution.
    std::array<double,9> off{},on{};
    std::uint64_t checksum=0;
    constexpr unsigned Replays=3000;
    for(unsigned sample=0;sample<10;++sample)for(unsigned order=0;order<2;++order) {
        const bool ahead=(sample+order)&1;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned n=0;n<Replays;++n)checksum+=Replay(Log23,ahead,Scenario::Shared).checksum;
        const double ns=std::chrono::duration<double,std::nano>(
            std::chrono::steady_clock::now()-start).count()/Replays;
        if(sample)(ahead?on:off)[sample-1]=ns;
    }
    std::ranges::sort(off);std::ranges::sort(on);
    std::printf("host-harness median ns/replay: baseline=%.0f ahead=%.0f ratio=%.3f "
                "checksum=%llu; no claim of device FPS or GPU-wait reduction\n",
                off[4],on[4],on[4]/off[4],(unsigned long long)checksum);
    std::puts("PASS: correctness and structural counters; GPU stall reduction NOT demonstrated.");
}
