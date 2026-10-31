// SPDX-License-Identifier: Apache-2.0
#include "region_edit.h"
#include <algorithm>
#include <limits>
namespace aitsnap::core {
bool contains(Rect r, Point p)
{
    return r.valid() && p.x >= r.x && p.y >= r.y && std::int64_t(p.x) < std::int64_t(r.x) + r.width && std::int64_t(p.y) < std::int64_t(r.y) + r.height;
}
Rect draggedRect(Rect r, Point start, Point end, RegionDrag mode)
{
    auto clamp = [](std::int64_t n) { return std::clamp(n, std::int64_t(std::numeric_limits<int>::min()), std::int64_t(std::numeric_limits<int>::max())); };
    const auto dx = std::int64_t(end.x) - start.x, dy = std::int64_t(end.y) - start.y;
    if (mode == RegionDrag::Move) return {
        int(std::min(clamp(std::int64_t(r.x) + dx), std::int64_t(std::numeric_limits<int>::max()) - r.width)),
        int(std::min(clamp(std::int64_t(r.y) + dy), std::int64_t(std::numeric_limits<int>::max()) - r.height)), r.width, r.height};
    std::int64_t left = r.x, top = r.y, right = std::int64_t(r.x) + r.width, bottom = std::int64_t(r.y) + r.height;
    if (mode == RegionDrag::Create) { left = start.x; top = start.y; right = end.x; bottom = end.y; }
    else {
        if (mode == RegionDrag::TopLeft || mode == RegionDrag::BottomLeft) left += dx; else right += dx;
        if (mode == RegionDrag::TopLeft || mode == RegionDrag::TopRight) top += dy; else bottom += dy;
    }
    if (left > right) std::swap(left, right);
    if (top > bottom) std::swap(top, bottom);
    const int width = int(std::min<std::int64_t>(std::max<std::int64_t>(1, right - left), std::numeric_limits<int>::max()));
    const int height = int(std::min<std::int64_t>(std::max<std::int64_t>(1, bottom - top), std::numeric_limits<int>::max()));
    return {int(clamp(left)), int(clamp(top)), width, height};
}
} // namespace aitsnap::core
