// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>

namespace aitsnap::core {
// Half-open physical desktop coordinates. Intermediates avoid signed overflow.
struct Rect {
    int x{}, y{}, width{}, height{};
    bool valid() const { return width > 0 && height > 0; }
    friend bool operator==(const Rect&, const Rect&) = default;
};
inline std::optional<Rect> intersect(Rect a, Rect b)
{
    if (!a.valid() || !b.valid()) return std::nullopt;
    const auto left = std::max<std::int64_t>(a.x, b.x);
    const auto top = std::max<std::int64_t>(a.y, b.y);
    const auto right = std::min(std::int64_t(a.x) + a.width, std::int64_t(b.x) + b.width);
    const auto bottom = std::min(std::int64_t(a.y) + a.height, std::int64_t(b.y) + b.height);
    if (right <= left || bottom <= top) return std::nullopt;
    return Rect{int(left), int(top), int(right - left), int(bottom - top)};
}
} // namespace aitsnap::core
