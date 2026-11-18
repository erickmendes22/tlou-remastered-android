// SPDX-License-Identifier: GPL-2.0-or-later
// Map a descriptor-info pointer back to a contiguous source snapshot without UB from
// relational comparisons of pointers into unrelated C++ arrays.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace Vulkan::Detail {
inline constexpr std::size_t InvalidDescriptorOffset = std::numeric_limits<std::size_t>::max();

template <typename T>
[[nodiscard]] std::size_t DescriptorSourceOffset(std::span<const T> source, const T* address,
                                                  std::size_t count) noexcept {
    if (!address || source.empty()) {
        return InvalidDescriptorOffset;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(source.data());
    const auto ptr = reinterpret_cast<std::uintptr_t>(address);
    if (ptr < base || (ptr - base) % sizeof(T) != 0) {
        return InvalidDescriptorOffset;
    }
    const auto index = static_cast<std::size_t>((ptr - base) / sizeof(T));
    if (index >= source.size() || count > source.size() - index) {
        return InvalidDescriptorOffset;
    }
    return index;
}
} // namespace Vulkan::Detail
