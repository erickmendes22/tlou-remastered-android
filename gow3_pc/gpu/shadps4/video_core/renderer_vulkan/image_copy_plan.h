// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace Vulkan::ImageCopyPlan {
using U32=std::uint32_t; using U64=std::uint64_t;
struct Image {
    U32 width,height,depth,layers,levels;
    bool volume;
    U32 block_width,block_height,block_bytes;
};
struct Region {
    U32 mip,src_width,src_height,src_depth,src_layers;
    U32 dst_width,dst_height,dst_depth,dst_layers;
    U64 bytes;
};
inline U32 Blocks(U32 size,U32 block) { return size/block+(size%block!=0); }
inline U32 Mip(U32 size,U32 mip) { return std::max(mip<32 ? size>>mip : 0u,1u); }
inline U64 BufferSpan(U32 width,U32 height,U32 depth,U32 layers,U32 row_length,
                      U32 image_height,U32 block_width,U32 block_height,U32 block_bytes) {
    if(!width||!height||!depth||!layers||!block_width||!block_height||!block_bytes) return 0;
    const U64 row=U64(Blocks(row_length ? row_length : width,block_width))*block_bytes;
    const U64 slice=U64(Blocks(image_height ? image_height : height,block_height))*row;
    return (U64(depth)*layers-1)*slice+(Blocks(height,block_height)-1)*row+
           U64(Blocks(width,block_width))*block_bytes;
}
// Bit reinterpretation of overlapping color/depth storage. Each side uses its
// own aspect/block layout; buffer rows have exactly the same byte footprint.
inline std::vector<Region> Build(const Image& src,const Image& dst) {
    if(!src.width||!dst.width||!src.height||!dst.height||!src.depth||!dst.depth||
       !src.layers||!dst.layers||!src.block_width||!dst.block_width||
       !src.block_height||!dst.block_height||!src.block_bytes||!dst.block_bytes) return {};
    std::vector<Region> regions;
    const U64 quantum=std::lcm<U64>(src.block_bytes,dst.block_bytes);
    for(U32 mip=0;mip<std::min(src.levels,dst.levels);++mip) {
        const U32 sw=Mip(src.width,mip),dw=Mip(dst.width,mip);
        const U32 sh=Mip(src.height,mip),dh=Mip(dst.height,mip);
        U64 row=std::min(U64(Blocks(sw,src.block_width))*src.block_bytes,
                         U64(Blocks(dw,dst.block_width))*dst.block_bytes);
        row-=row%quantum;
        const U32 rows=std::min(Blocks(sh,src.block_height),Blocks(dh,dst.block_height));
        const U32 slices=std::min(src.volume ? Mip(src.depth,mip) : src.layers,
                                  dst.volume ? Mip(dst.depth,mip) : dst.layers);
        if(!row||!rows||!slices) continue;
        regions.push_back({mip,
            U32(std::min<U64>(sw,row/src.block_bytes*src.block_width)),
            std::min(sh,rows*src.block_height),src.volume ? slices : 1u,src.volume ? 1u : slices,
            U32(std::min<U64>(dw,row/dst.block_bytes*dst.block_width)),
            std::min(dh,rows*dst.block_height),dst.volume ? slices : 1u,dst.volume ? 1u : slices,
            row*rows*slices});
    }
    return regions;
}
} // namespace Vulkan::ImageCopyPlan
