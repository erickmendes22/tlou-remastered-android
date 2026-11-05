"""Native C++ pointer range validation for TLOU deferred descriptor snapshots."""
from pathlib import Path
import subprocess
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "gow3_pc/gpu/shadps4/video_core/renderer_vulkan/vk_descriptor_record.h"

SOURCE = r"""
#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include "vk_descriptor_record.h"
int main() {
    struct Buffer { unsigned long long handle, offset, size; };
    struct Image { unsigned long long sampler, view, layout; };
    std::array<Buffer, 4> buffers{{{1,2,3},{4,5,6},{7,8,9},{10,11,12}}};
    std::array<Buffer, 4> unrelated = buffers;
    std::array<Image, 3> images{{{1,2,3},{4,5,6},{7,8,9}}};
    std::span<const Buffer> bs = buffers;
    std::span<const Image> is = images;
    using Vulkan::Detail::DescriptorSourceOffset;
    using Vulkan::Detail::InvalidDescriptorOffset;
    assert(DescriptorSourceOffset(bs, buffers.data(), 1) == 0);
    assert(DescriptorSourceOffset(bs, buffers.data()+1, 2) == 1);
    assert(DescriptorSourceOffset(bs, buffers.data()+3, 2) == InvalidDescriptorOffset);
    assert(DescriptorSourceOffset(bs, static_cast<const Buffer*>(nullptr), 1) == InvalidDescriptorOffset);
    assert(DescriptorSourceOffset(bs, unrelated.data(), 1) == InvalidDescriptorOffset);
    assert(DescriptorSourceOffset(is, images.data()+2, 1) == 2);
    assert(DescriptorSourceOffset(is, images.data()+3, 0) == InvalidDescriptorOffset);
    auto* misaligned = reinterpret_cast<const Buffer*>(
        reinterpret_cast<const std::byte*>(buffers.data()) + 1);
    assert(DescriptorSourceOffset(bs, misaligned, 1) == InvalidDescriptorOffset);
}
"""

class DescriptorSnapshot(unittest.TestCase):
    def test_range_validation_compiles_and_runs(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        self.assertTrue(HEADER.exists())
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "check.cpp"
            exe = Path(tmp) / "check"
            source.write_text(SOURCE)
            subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra",
                            "-Werror", "-I", str(HEADER.parent), str(source),
                            "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    unittest.main()
