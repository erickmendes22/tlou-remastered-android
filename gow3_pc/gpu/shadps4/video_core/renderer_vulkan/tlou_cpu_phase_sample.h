// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <ctime>
#include <utility>

namespace Vulkan::Detail {
struct ThreadCpuSampleClock {
    static std::uint64_t WallNs() noexcept {
#ifdef CLOCK_MONOTONIC
        timespec t{};
        if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
        return std::uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;
#else
        return 0;
#endif
    }
    static std::uint64_t CpuNs() noexcept {
#ifdef CLOCK_THREAD_CPUTIME_ID
        timespec t{};
        if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))return 0;
        return std::uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;
#else
        return 0; // Unsupported host: drop sample rather than substitute wall time.
#endif
    }
};

// One instance per role per thread. Idle draws only decrement a counter. No
// timestamp queries, GPU queries, locks or output in that common path.
template<std::size_t Phases,class Clock=ThreadCpuSampleClock>
class CpuPhaseSampler {
public:
    static constexpr unsigned MaxSamplesPerWindow=8;
    static constexpr unsigned MaxMarks=32;
    struct Stats {
        std::uint64_t calls{},samples{},dropped{},cpu_reads{};
        std::array<std::uint64_t,Phases> cpu_ns{};
    };
    class Sample {
    public:
        Sample() noexcept = default;
        Sample(const Sample&)=delete;
        Sample& operator=(const Sample&)=delete;
        Sample(Sample&& other) noexcept
            : stats(std::exchange(other.stats,nullptr)),spent(other.spent),
              previous(other.previous),phase(other.phase),marks(other.marks),valid(other.valid) {}
        ~Sample(){Finish();}
        bool Active()const noexcept{return stats && valid;}
        void Phase(std::size_t next) noexcept {
            if(!stats || !valid)return;
            if(next>=Phases || ++marks>MaxMarks){valid=false;return;}
            const auto now=Read();
            if(!now || now<previous){valid=false;return;}
            spent[phase]+=now-previous;previous=now;phase=next;
        }
        void Finish() noexcept {
            if(!stats)return;
            if(valid) {
                const auto now=Read();
                if(now && now>=previous) {
                    spent[phase]+=now-previous;
                    for(std::size_t p=0;p<Phases;++p)stats->cpu_ns[p]+=spent[p];
                    ++stats->samples;
                }else valid=false;
            }
            if(!valid)++stats->dropped;
            stats=nullptr;
        }
    private:
        friend class CpuPhaseSampler;
        explicit Sample(Stats* s) noexcept:stats(s) {
            spent.fill(0);previous=Read();valid=previous!=0;
        }
        std::uint64_t Read()noexcept{++stats->cpu_reads;return Clock::CpuNs();}
        Stats* stats=nullptr;
        std::array<std::uint64_t,Phases> spent{};
        std::uint64_t previous=0;
        std::size_t phase=0;
        unsigned marks=0;
        bool valid=true;
    };
    explicit CpuPhaseSampler(bool enabled=true,unsigned interval=128,unsigned mask=255)
        : enabled(enabled),base_interval(interval?interval:1),interval_mask(mask) {}

    template<class Report>Sample Begin(Report&& report) {
        if(!enabled)return {};
        ++stats.calls;
        if(--countdown)return {};
        random^=random<<13;random^=random>>17;random^=random<<5;
        countdown=base_interval+(random&interval_mask);
        const auto now=Clock::WallNs();
        if(!now)return {};
        if(!report_at)report_at=now;
        if(now-report_at>=5000000000ull) {
            --stats.calls;
            report(stats,now-report_at);stats={};stats.calls=1;report_at=now;
        }
        if(!budget_at || now-budget_at>=1000000000ull){budget_at=now;budget=0;}
        // Spread opportunities across the second; do not spend the entire
        // budget in its first few frames when calls arrive in a burst.
        if(budget>=MaxSamplesPerWindow || now<next_sample_at)return {};
        ++budget;
        next_sample_at=now+125000000ull;
        return Sample{&stats};
    }
    const Stats& Current()const noexcept{return stats;}
private:
    bool enabled;
    unsigned base_interval,interval_mask,countdown=1,budget=0;
    std::uint32_t random=0x6d2b79f5;
    std::uint64_t report_at=0,budget_at=0,next_sample_at=0;
    Stats stats{};
};

// Binding is thread-local: a helper thread must never charge CPU time to its
// caller's sample. Restore the previous binding on nested/early-return scopes.
template<class Sample>
class CpuSampleBinding {
public:
    explicit CpuSampleBinding(Sample& sample):previous(current) {
        current=sample.Active()?&sample:nullptr;
    }
    CpuSampleBinding(const CpuSampleBinding&)=delete;
    CpuSampleBinding& operator=(const CpuSampleBinding&)=delete;
    ~CpuSampleBinding(){current=previous;}
    static void Phase(unsigned phase)noexcept{if(current)current->Phase(phase);}
private:
    static inline thread_local Sample* current=nullptr;
    Sample* previous;
};
} // namespace Vulkan::Detail
