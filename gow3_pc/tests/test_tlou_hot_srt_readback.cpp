// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/buffer_cache/tlou_hot_srt_readback.h"
#include <cassert>
#include <cstdio>
using namespace TlouHotSrtReadback;
static_assert(Eligible(8,8,9,112,4096));
static_assert(Eligible(6,8,9,4,4096));
static_assert(!Eligible(0,8,9,112,4096));
static_assert(!Eligible(9,8,10,112,4096));
static_assert(!Eligible(8,8,8,112,4096));
static_assert(!Eligible(8,8,9,4097,4096));
static_assert(!Eligible(8,8,9,0,4096));
static_assert(PublishAllowed(false,false,false,true));
static_assert(!PublishAllowed(true,false,false,true));
static_assert(!PublishAllowed(false,true,false,true));
static_assert(!PublishAllowed(false,false,true,true));
static_assert(!PublishAllowed(false,false,false,false));
static_assert(PagesDisjoint(0x1000,112,0x2000,112));
static_assert(!PagesDisjoint(0x1000,112,0x1100,112));
static_assert(!PagesDisjoint(0x1ff0,112,0x2000,112));
static_assert(PagesDisjoint(0x3000,16,0x1000,16));
static_assert(Overlaps(0,64,32,64));
static_assert(!Overlaps(0,32,32,64));
int main(){
    for(unsigned primary=1;primary<16;++primary)
      for(unsigned now=1;now<20;++now)
        for(unsigned cand=0;cand<20;++cand){
          const bool expected=cand>0 && cand<=primary && primary<now;
          assert(Eligible(cand,primary,now,4,4096)==expected);
        }
    for(int flags=0;flags<16;++flags)
        assert(PublishAllowed(flags&1,flags&2,flags&4,!(flags&8))==(flags==0));
    std::puts("TLOU temporal hot SRT producer and late CPU-write gates: PASS");
}
