// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstring>
// A fiber moves guest registers, never the executing FEX thread's JIT lookup
// cache, call/return stack, signal counters, segment tables or FS/GS TLS bases.
// This also applies to restoring the root on the same thread after callbacks.
template<class State>
void RestoreFiberRegisters(State& current, const State& saved) {
    current.rip = saved.rip;
    std::memcpy(current.gregs, saved.gregs, sizeof(current.gregs));
    current.pf_raw = saved.pf_raw;
    current.af_raw = saved.af_raw;
    std::memcpy(current.avx_high, saved.avx_high, sizeof(current.avx_high));
    std::memcpy(&current.xmm, &saved.xmm, sizeof(current.xmm));
    std::memcpy(current.flags, saved.flags, sizeof(current.flags));
    std::memcpy(current.mm, saved.mm, sizeof(current.mm));
    current.mxcsr = saved.mxcsr;
    current.FCW = saved.FCW;
    current.AbridgedFTW = saved.AbridgedFTW;
}
