// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/amdgpu/pm4_packet_checksum.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
using namespace AmdGpu::Detail;
using Hash=std::uint64_t(*)(std::uint64_t,const std::uint32_t*,std::uint32_t);
double Measure(Hash hash,std::uint32_t count,std::uint64_t& result) {
    std::vector<std::uint32_t> words(count*64);
    std::mt19937 rng(31);for(auto& w:words)w=rng();
    constexpr unsigned Packets=200000;std::uint64_t checksum=12345;
    const auto before=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Packets;++i)checksum=hash(checksum,words.data()+(i%64)*count,count);
    result=checksum;
    return std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-before).count()/Packets;
}
int main(int argc,char** argv) {
    assert(PacketChecksum(123,nullptr,0)==123);
    std::mt19937 rng(17);
    for(unsigned trial=0;trial<100000;++trial) {
        std::array<std::uint32_t,256> a{},b{};const auto count=1+rng()%256;
        for(auto& w:a)w=rng();
        b=a;
        const std::uint64_t seed=(std::uint64_t(rng())<<32)|rng();
        const auto x=PacketChecksum(seed,a.data(),count);
        assert(x==PacketChecksum(seed,b.data(),count));
        b[rng()%count]^=1u<<(rng()%32);
        assert(x!=PacketChecksum(seed,b.data(),count));
        assert(x!=PacketChecksum(seed^1,a.data(),count));
        if(count<=7)assert(x==LegacyPacketChecksum(seed,a.data(),count));
    }
    const std::array<std::uint32_t,8> first{1,2,3,4,5,6,7,8},second{9,10,11,12,13,14,15,16};
    assert(PacketChecksum(PacketChecksum(9,first.data(),8),second.data(),8)!=
           PacketChecksum(PacketChecksum(9,second.data(),8),first.data(),8));
#ifndef _WIN32
    const auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* mapping=static_cast<std::uint32_t*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(mapping!=MAP_FAILED);
    assert(!mprotect(reinterpret_cast<char*>(mapping)+page,page,PROT_NONE));
    for(std::size_t i=0;i<page/4;++i)mapping[i]=rng();
    for(unsigned count=1;count<=page/4;++count) {
        auto* end_aligned=mapping+page/4-count;
        const std::vector<std::uint32_t> copy(end_aligned,end_aligned+count);
        assert(PacketChecksum(987,end_aligned,count)==PacketChecksum(987,copy.data(),count));
    }
    assert(!munmap(mapping,page*2));
#endif
    std::puts("PASS: identical replay, short-path equivalence, empty input, 100000 mutated packet/seed cases (not a proof of collision absence)");
    std::puts("PASS: packet ordering and every input length ending at an inaccessible page boundary");
    std::printf("XXHash version=%u\n",XXH_versionNumber());
    if(argc==2 && !std::strcmp(argv[1],"--correctness-only"))return 0;
    std::uint64_t result=0;
    for(unsigned count:{3u,4u,5u,6u,7u,8u,16u,32u,64u,256u,1024u}) {
        std::array<double,9> a{},b{};
        for(unsigned sample=0;sample<10;++sample){double x,y;std::uint64_t h1,h2;
            if(sample&1){y=Measure(PacketChecksum,count,h2);x=Measure(LegacyPacketChecksum,count,h1);}
            else{x=Measure(LegacyPacketChecksum,count,h1);y=Measure(PacketChecksum,count,h2);}
            result+=h1^(h2+sample);
            if(sample){a[sample-1]=x;b[sample-1]=y;}}
        std::ranges::sort(a);std::ranges::sort(b);
        std::printf("words=%u median ns/packet baseline=%.2f candidate=%.2f ratio=%.3f\n",count,a[4],b[4],b[4]/a[4]);
    }
    std::printf("checksum sink=%llu; host CPU packet-checksum cost, not FPS\n",(unsigned long long)result);
}
