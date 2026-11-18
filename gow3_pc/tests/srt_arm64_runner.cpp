// Standalone harness for the production ARM64 emitter and portable walker.
#include "../gpu/shadps4/shader_recompiler/ir/passes/srt_arm64.h"
#include <iostream>
#include <cstring>
#include <array>
#include <stdexcept>
using namespace Shader::Arm64Srt;
using U32=std::uint32_t;
int main() {
    std::vector<U32> program; U32 word;
    while(std::cin>>word) program.push_back(word);
    auto code=Compile(program);
    if(code.empty()) { std::cout<<"{}"; return 0; }
    std::array<U32,16> user{};
    user[0]=0x30000; user[1]=0xabcd0000; user[2]=3; user[3]=7;
    std::array<U32,256> flat; flat.fill(0xdeadbeef);
    std::array<U32,256> table{},child{};
    for(size_t i=0;i<table.size();++i) { table[i]=i*37+11; child[i]=i*71+19; }
    table[8]=0x40000; table[9]=0;
    auto read=[&](const void* ptr,size_t bytes)->std::uint64_t {
        auto address=reinterpret_cast<std::uintptr_t>(ptr);
        std::uint64_t value=0;
        auto base=reinterpret_cast<std::uintptr_t>(user.data());
        if(address>=base && address+bytes<=base+sizeof(user)) std::memcpy(&value,ptr,bytes);
        else if(address>=0x30000 && address+bytes<=0x30000+sizeof(table)) std::memcpy(&value,reinterpret_cast<const char*>(table.data())+address-0x30000,bytes);
        else if(address>=0x40000 && address+bytes<=0x40000+sizeof(child)) std::memcpy(&value,reinterpret_cast<const char*>(child.data())+address-0x40000,bytes);
        else throw std::runtime_error("unexpected table read");
        return value;
    };
    Interpret(program.data(),user.data(),flat.data(),read);
    auto print=[](const auto& values) {
        std::cout<<'['; bool comma=false;
        for(auto v:values) { if(comma) std::cout<<','; std::cout<<v; comma=true; }
        std::cout<<']';
    };
    std::cout<<"{\"code\":";print(code);
    std::cout<<",\"user\":";print(user);
    std::cout<<",\"table\":";print(table);
    std::cout<<",\"child\":";print(child);
    std::cout<<",\"expected\":";print(flat); std::cout<<'}';
}
