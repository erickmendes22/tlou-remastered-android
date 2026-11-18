// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/renderer_vulkan/tlou_cpu_phase_sample.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using namespace Vulkan::Detail;
struct FakeClock {
    static inline std::uint64_t wall=100,cpu=1000,wall_calls=0,cpu_calls=0;
    static std::uint64_t WallNs(){++wall_calls;return wall;}
    static std::uint64_t CpuNs(){++cpu_calls;return cpu;}
};
void Correctness() {
    using S=CpuPhaseSampler<3,FakeClock>;
    auto report=[](const auto&,auto){};
    S exact{true,1,0};
    {
        auto p=exact.Begin(report);assert(p.Active());
        FakeClock::cpu+=100;p.Phase(1);
        FakeClock::cpu+=200;p.Phase(2);
        FakeClock::cpu+=300;
        auto moved=std::move(p);moved.Finish();moved.Finish();
    }
    assert(exact.Current().samples==1 && exact.Current().cpu_ns[0]==100 &&
           exact.Current().cpu_ns[1]==200 && exact.Current().cpu_ns[2]==300);
    assert(exact.Current().cpu_reads==4);
    S budget{true,1,0};
    for(unsigned i=0;i<100000;++i){auto p=budget.Begin(report);p.Phase(1);p.Phase(2);}
    assert(budget.Current().calls==100000 && budget.Current().samples==1);
    for(unsigned i=0;i<7;++i){FakeClock::wall+=125000000ull;
        auto p=budget.Begin(report);p.Phase(1);p.Phase(2);}
    assert(budget.Current().samples==8);
    assert(budget.Current().cpu_reads==32);
    for(unsigned i=0;i<8;++i){FakeClock::wall+=125000000ull;
        auto p=budget.Begin(report);p.Phase(1);p.Phase(2);}
    assert(budget.Current().samples==16 && budget.Current().cpu_reads==64);
    auto saved=budget.Current();bool reported=false;
    FakeClock::wall+=5000000000ull;
    {
        auto p=budget.Begin([&](const auto& stats,auto ns){
            assert(ns>=5000000000ull && stats.calls==saved.calls && stats.samples==16);
            reported=true;
        });
        assert(reported && budget.Current().calls==1);
    }
    assert(budget.Current().samples==1);
    S broken{true,1,0};
    {auto p=broken.Begin(report);FakeClock::cpu=0;p.Phase(1);}
    assert(broken.Current().samples==0 && broken.Current().dropped==1);
    FakeClock::cpu=2000;
    S excessive{true,1,0};
    {auto p=excessive.Begin(report);for(unsigned i=0;i<1000;++i)p.Phase(i%3);}
    assert(excessive.Current().samples==0 && excessive.Current().dropped==1);
    assert(excessive.Current().cpu_reads<=S::MaxMarks+2);
    auto old_cpu=FakeClock::cpu_calls,old_wall=FakeClock::wall_calls;
    S off{false,1,0};
    for(unsigned i=0;i<10000;++i){auto p=off.Begin(report);p.Phase(2);}
    assert(!off.Current().calls && FakeClock::cpu_calls==old_cpu && FakeClock::wall_calls==old_wall);
    S bound{true,1,0};
    using Binding=CpuSampleBinding<S::Sample>;
    {
        auto p=bound.Begin(report);Binding active{p};
        const auto reads=FakeClock::cpu_calls;
        {auto disabled=off.Begin(report);Binding nested{disabled};Binding::Phase(1);}
        std::jthread helper([]{Binding::Phase(1);});helper.join();
        assert(FakeClock::cpu_calls==reads);
        FakeClock::cpu+=11;Binding::Phase(1);
    }
    assert(bound.Current().samples==1 && bound.Current().cpu_ns[0]==11);
    old_cpu=FakeClock::cpu_calls;Binding::Phase(0);assert(FakeClock::cpu_calls==old_cpu);
    std::puts("PASS: exact phase attribution, rate cap, clock-read cap, reporting/reset, "
              "failed clocks, excessive phases, move/early finish, nested/thread-local bindings "
              "and zero clock calls when OFF");
}
struct BenchResult {double ns;std::uint64_t sink,reads,samples;};
inline void OpaqueBoundary() {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" ::: "memory");
#endif
}
struct BudgetCostClock {
    static inline std::uint64_t wall=100;
    static std::uint64_t WallNs(){wall+=125000000ull;return wall;}
    static std::uint64_t CpuNs(){return ThreadCpuSampleClock::CpuNs();}
};
double SelectedScopeCost() {
    CpuPhaseSampler<6,BudgetCostClock> sampler{true,1,0};
    constexpr unsigned Selected=10000;
    const auto before=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Selected;++i) {
        auto p=sampler.Begin([](const auto&,auto){});assert(p.Active());
        for(unsigned phase=0;phase<32;++phase)p.Phase(phase%6);
    }
    return std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-before).count()/Selected;
}
BenchResult Measure(bool enabled,bool opaque=false) {
    CpuPhaseSampler<6> sampler{enabled};
    using Binding=CpuSampleBinding<CpuPhaseSampler<6>::Sample>;
    constexpr unsigned Draws=2000000;
    std::uint64_t work=8;
    const auto before=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Draws;++i) {
        auto p=sampler.Begin([](const auto&,auto){});
        Binding binding{p};
        // 16 markers covers the normal multi-stage buffer/texture sequence.
        // A dependent work stream prevents the empty loop being optimized out.
        for(unsigned phase=0;phase<16;++phase){
            work=(work^i^phase)*0x9e3779b97f4a7c15ull;
            if(opaque)OpaqueBoundary();
            Binding::Phase(phase%6);
        }
    }
    const double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-before).count()/Draws;
    const auto s=sampler.Current();
    return {ns,work,s.cpu_reads,s.samples};
}
BenchResult MeasureBare(bool opaque=false) {
    constexpr unsigned Draws=2000000;
    std::uint64_t work=8;
    const auto before=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Draws;++i)for(unsigned phase=0;phase<16;++phase) {
        work=(work^i^phase)*0x9e3779b97f4a7c15ull;
        if(opaque)OpaqueBoundary();
    }
    return {std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-before).count()/Draws,
            work,0,0};
}
int main(int argc,char** argv) {
    Correctness();
    if(argc==2 && !std::strcmp(argv[1],"--correctness-only"))return 0;
    std::array<double,9> bare{},off{},on{};std::uint64_t sink=0;
    for(unsigned sample=0;sample<10;++sample) {
        BenchResult a,b;
        if(sample&1){b=Measure(true);a=Measure(false);}else{a=Measure(false);b=Measure(true);}
        assert(a.sink==b.sink && !a.reads);sink+=a.sink+b.sink;
        const auto plain=MeasureBare();assert(plain.sink==a.sink);
        if(sample){bare[sample-1]=plain.ns;off[sample-1]=a.ns;on[sample-1]=b.ns;}
        assert(b.reads==b.samples*18);
    }
    std::ranges::sort(bare);std::ranges::sort(off);std::ranges::sort(on);
    std::printf("real-clock sampler median ns/draw bare=%.2f OFF=%.2f ON=%.2f added-vs-bare=%.2f "
                "(16 markers; 8 samples per role per 1s budget; no GPU queries) sink=%llu\n",
                bare[4],off[4],on[4],on[4]-bare[4],(unsigned long long)sink);
    std::array<double,5> selected{};for(auto& ns:selected)ns=SelectedScopeCost();
    std::ranges::sort(selected);
    std::printf("clock-budget-case median ns/selected-scope=%.1f "
                "(32 markers, 34 real CPU-clock reads, synthetic wall progression; "
                "not total production instrumentation cost)\n",selected[2]);
    std::array<double,5> opaque_bare{},opaque_on{};
    for(unsigned i=0;i<6;++i){
        BenchResult a,b;
        if(i&1){b=Measure(true,true);a=MeasureBare(true);}
        else{a=MeasureBare(true);b=Measure(true,true);}
        assert(a.sink==b.sink);
        if(i){opaque_bare[i-1]=a.ns;opaque_on[i-1]=b.ns;}}
    std::ranges::sort(opaque_bare);std::ranges::sort(opaque_on);
    std::printf("opaque-boundary sampler ns/draw bare=%.2f ON=%.2f added=%.2f "
                "(compiler cannot hoist thread-local marker loads across boundaries)\n",
                opaque_bare[2],opaque_on[2],opaque_on[2]-opaque_bare[2]);
}
