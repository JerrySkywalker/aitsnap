// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "domain.h"
namespace aitsnap::core {
struct Point { int x{}, y{}; };
enum class RegionDrag { Create, Move, TopLeft, TopRight, BottomLeft, BottomRight };
Rect draggedRect(Rect original, Point start, Point current, RegionDrag mode);
bool contains(Rect rect, Point point);
} // namespace aitsnap::core
