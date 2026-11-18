// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/renderer_vulkan/tlou_srt_batch_plan.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <span>
#include <vector>

int main() {
    using namespace Vulkan::TlouSrtBatch;
    static_assert(Fits(0, 0, 16));
    static_assert(Fits(7, 64, 16));
    static_assert(!Fits(8, 0, 16));
    static_assert(!Fits(0, 0, 0));
    static_assert(Fits(0, 0, 65536));
    static_assert(!Fits(1, 65535, 2));
    static_assert(!Fits(0, 0, 65537));
    std::array<int, 3> siblings{0, 2, 4};
    const std::span<const int> pending{siblings};
    assert(ParentPending(0, pending));
    assert(ParentPending(2, pending));
    assert(ParentPending(4, pending));
    assert(!ParentPending(-1, pending));
    assert(!ParentPending(1, pending));
    assert(!ParentPending(8, pending));

    // Model the same topologically ordered parent graph as SrtPlanFor:
    // batching siblings is safe, but before computing any dependent pointer
    // their parent table must have been published to guest memory.
    struct Node { int parent; unsigned size; bool gpu_written; };
    const std::array<Node, 9> nodes{{
        {-1, 16, true},   // parent of 1,2 -> must land first
        { 0, 16, true},   // 1 and 2 are siblings
        { 0, 16, true},
        {-1, 16, true},   // root 3 and 4 can join previous siblings
        {-1, 16, true},
        { 2, 16, true},   // must flush the group containing node 2
        { 3, 16, false},  // CPU-owned: no download required
        { 5, 16, true},
        { 7, 16, true},   // must flush node 7 before resolving 8
    }};
    std::vector<int> pending_nodes;
    std::array<bool, 9> published{};
    std::array<unsigned, 9> flush_count{};
    std::size_t groups = 0, batched = 0;
    const auto flush = [&] {
        if (pending_nodes.empty()) return;
        ++groups;
        batched += pending_nodes.size() - 1;
        for (const int i : pending_nodes) {
            ++flush_count[i];
            published[i] = true;
        }
        pending_nodes.clear();
    };
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        if (ParentPending(node.parent, std::span<const int>{pending_nodes}))
            flush();
        if (node.parent >= 0)
            assert(published[node.parent]); // resolved pointer is fresh
        if (!node.gpu_written) {
            published[i] = true;
            continue;
        }
        if (!Fits(pending_nodes.size(), pending_nodes.size()*16, node.size))
            flush();
        pending_nodes.push_back(static_cast<int>(i));
    }
    flush();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        assert(flush_count[i] == (nodes[i].gpu_written ? 1u : 0u));
        assert(published[i]);
    }
    assert(groups >= 2);
    assert(batched > 0); // verifies a real grouped readback
    std::puts("TLOU SRT dependency-aware batch tests: PASS");
}
