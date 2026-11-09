#include "../gpu/shadps4/video_core/renderer_vulkan/image_copy_plan.h"
#include <cassert>
#include <random>
#include <iostream>
using namespace Vulkan::ImageCopyPlan;
static void Check(const Image& a,const Image& b) {
    auto regions=Build(a,b);
    for(auto r:regions) {
        assert(r.mip<std::min(a.levels,b.levels));
        assert(r.src_width<=Mip(a.width,r.mip) && r.dst_width<=Mip(b.width,r.mip));
        assert(r.src_height<=Mip(a.height,r.mip) && r.dst_height<=Mip(b.height,r.mip));
        assert(a.volume ? r.src_layers==1 && r.src_depth<=Mip(a.depth,r.mip) : r.src_depth==1 && r.src_layers<=a.layers);
        assert(b.volume ? r.dst_layers==1 && r.dst_depth<=Mip(b.depth,r.mip) : r.dst_depth==1 && r.dst_layers<=b.layers);
        assert(r.bytes==BufferSpan(r.src_width,r.src_height,r.src_depth,r.src_layers,0,0,a.block_width,a.block_height,a.block_bytes));
        assert(r.bytes==BufferSpan(r.dst_width,r.dst_height,r.dst_depth,r.dst_layers,0,0,b.block_width,b.block_height,b.block_bytes));
    }
}
int main() {
    Image a{1920,1080,1,1,4,false,1,1,4},b{1920,1080,1,2,4,false,1,1,4};
    auto regions=Build(a,b);
    assert(regions.size()==4 && regions[0].src_layers==1 && regions[0].dst_layers==1);
    assert(regions[0].bytes==1920ULL*1080*4);
    Check(a,b);
    Image volume{128,64,8,1,4,true,1,1,4},array{128,64,1,6,4,false,1,1,4};
    auto slices=Build(volume,array);
    assert(slices[0].src_depth==6 && slices[0].dst_layers==6);
    assert(slices[1].src_depth==4 && slices[1].dst_layers==4);
    Check(volume,array); Check(array,volume);
    Image d16{128,64,1,1,1,false,1,1,2},rgba{64,64,1,1,1,false,1,1,4};
    auto bits=Build(d16,rgba);
    assert(bits[0].src_width==128 && bits[0].dst_width==64 && bits[0].bytes==16384);
    Check(d16,rgba);
    assert(BufferSpan(5,7,1,1,0,0,4,4,16)==64); // partial BC blocks
    assert(BufferSpan(3,2,2,1,8,4,1,1,4)==172); // padded rows and slices
    assert(BufferSpan(65536,65536,1,2,0,0,1,1,4)==34359738368ULL);
    assert(BufferSpan(0,1,1,1,0,0,1,1,4)==0);
    std::mt19937 rng(552);
    for(int i=0;i<4000;++i) {
        auto image=[&] {
            return Image{U32(1+rng()%256),U32(1+rng()%256),U32(1+rng()%16),U32(1+rng()%8),U32(1+rng()%6),
                         bool(rng()%2),rng()%2 ? 4u:1u,rng()%2 ? 4u:1u,2u<<unsigned(rng()%4)};
        };
        auto x=image(),y=image(); Check(x,y);
    }
    std::cout<<"image-copy-plan: unequal layers, mips, 2D/3D, aspect sizes and buffer bounds passed\n";
}
