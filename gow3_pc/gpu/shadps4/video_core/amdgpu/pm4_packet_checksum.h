// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <xxhash.h>

namespace AmdGpu::Detail {
inline std::uint64_t LegacyPacketChecksum(std::uint64_t checksum,
                                         const std::uint32_t* words,std::uint32_t count) {
    for(std::uint32_t i=0;i<count;++i) {
        checksum=(checksum^words[i])*0x9E3779B97F4A7C15ull;
        checksum^=checksum>>29;
    }
    return checksum;
}
// A process-local speculative-preparation identity, not a persisted cache key.
// Short packets retain the old path; larger payloads avoid one serial dependent
// multiply/shift chain per word. Every producer/consumer uses this same helper.
inline std::uint64_t PacketChecksum(std::uint64_t checksum,
                                   const std::uint32_t* words,std::uint32_t count) {
    if(count<=7)return LegacyPacketChecksum(checksum,words,count);
    return XXH3_64bits_withSeed(words,std::size_t(count)*sizeof(*words),checksum);
}
} // namespace AmdGpu::Detail
