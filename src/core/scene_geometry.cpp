// SPDX-License-Identifier: Apache-2.0
#include "scene_geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace aitsnap::core {
SceneTemplate presetScene(const CaptureProfile& profile, std::string id, std::string name, int width, int height, LayoutPreset preset)
{
    SceneTemplate scene; scene.id = std::move(id); scene.name = std::move(name); scene.width = width; scene.height = height;
    std::vector<Region> regions; for (const auto& r : profile.regions) if (r.enabled) regions.push_back(r);
    const int n = int(regions.size()); if (!n) return scene;
    const double pad = 16, gap = 12, availableWidth = width - 2 * pad, availableHeight = height - 2 * pad;
    int columns = preset == LayoutPreset::Row ? n : preset == LayoutPreset::Column ? 1 : int(std::ceil(std::sqrt(double(n))));
    const int rows = (n + columns - 1) / columns;
    for (int i = 0; i < n; ++i) {
        Layer layer; layer.id = "layer-" + regions[std::size_t(i)].id; layer.sourceRegionId = regions[std::size_t(i)].id; layer.zIndex = i;
        const double w = (availableWidth - (columns - 1) * gap) / columns, h = (availableHeight - (rows - 1) * gap) / rows;
        layer.destination = {pad + (i % columns) * (w + gap), pad + (i / columns) * (h + gap), w, h, 0};
        if (preset == LayoutPreset::Hero && n > 1) {
            if (i == 0) layer.destination = {pad, pad, availableWidth * 0.68 - gap, availableHeight, 0};
            else { const double smallH = (availableHeight - (n - 2) * gap) / (n - 1);
                layer.destination = {pad + availableWidth * 0.68, pad + (i - 1) * (smallH + gap), availableWidth * 0.32, smallH, 0}; }
        }
        scene.layers.push_back(layer);
    }
    return scene;
}
void alignLayers(SceneTemplate& scene, const std::vector<std::size_t>& selected, Align align)
{
    if (selected.empty()) return;
    double left = std::numeric_limits<double>::infinity(), top = left, right = -left, bottom = -left;
    for (auto index : selected) if (index < scene.layers.size()) { const auto& d = scene.layers[index].destination;
        left = std::min(left, d.x); top = std::min(top, d.y); right = std::max(right, d.x + d.width); bottom = std::max(bottom, d.y + d.height); }
    if (selected.size() == 1) { left = 0; top = 0; right = scene.width; bottom = scene.height; }
    for (auto index : selected) if (index < scene.layers.size() && !scene.layers[index].locked) {
        auto& d = scene.layers[index].destination;
        switch (align) {
        case Align::Left: d.x = left; break; case Align::Right: d.x = right - d.width; break;
        case Align::Top: d.y = top; break; case Align::Bottom: d.y = bottom - d.height; break;
        case Align::CentreX: d.x = (left + right - d.width) / 2; break; case Align::CentreY: d.y = (top + bottom - d.height) / 2; break;
        }
    }
}
void distributeLayers(SceneTemplate& scene, const std::vector<std::size_t>& selected, bool horizontal)
{
    auto indices = selected;
    indices.erase(std::remove_if(indices.begin(), indices.end(), [&](auto i) { return i >= scene.layers.size() || scene.layers[i].locked; }), indices.end());
    if (indices.size() < 3) return;
    std::sort(indices.begin(), indices.end(), [&](auto a, auto b) { const auto& x = scene.layers[a].destination; const auto& y = scene.layers[b].destination; return horizontal ? x.x < y.x : x.y < y.y; });
    const auto& first = scene.layers[indices.front()].destination; const auto& last = scene.layers[indices.back()].destination;
    const double start = horizontal ? first.x : first.y, end = horizontal ? last.x + last.width : last.y + last.height;
    double occupied = 0; for (auto i : indices) occupied += horizontal ? scene.layers[i].destination.width : scene.layers[i].destination.height;
    const double gap = (end - start - occupied) / (indices.size() - 1); double position = start;
    for (auto i : indices) { auto& d = scene.layers[i].destination; if (horizontal) d.x = position; else d.y = position; position += (horizontal ? d.width : d.height) + gap; }
}
} // namespace aitsnap::core
