// SPDX-License-Identifier: Apache-2.0
#include "scene_geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
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

namespace {
constexpr double canvasMargin = 16;

struct Bounds {
    double left{}, top{}, right{}, bottom{};
    bool empty() const { return right <= left || bottom <= top; }
    double width() const { return right - left; }
    double height() const { return bottom - top; }
    void include(const Bounds& other)
    {
        if (empty()) { *this = other; return; }
        left = std::min(left, other.left);
        top = std::min(top, other.top);
        right = std::max(right, other.right);
        bottom = std::max(bottom, other.bottom);
    }
};

Bounds rotatedBounds(const Destination& d)
{
    const auto radians = std::fmod(d.rotationDeg, 360) * std::numbers::pi / 180;
    const auto cosine = std::abs(std::cos(radians)), sine = std::abs(std::sin(radians));
    const auto halfWidth = (d.width * cosine + d.height * sine) / 2;
    const auto halfHeight = (d.width * sine + d.height * cosine) / 2;
    const auto centreX = d.x + d.width / 2, centreY = d.y + d.height / 2;
    return {centreX - halfWidth, centreY - halfHeight, centreX + halfWidth, centreY + halfHeight};
}

double snapIntegralEdge(double edge)
{
    const auto rounded = std::round(edge);
    return std::abs(edge - rounded) < 1e-8 ? rounded : edge;
}
} // namespace

Destination fittedLayerBox(const Layer& layer, int sourceWidth, int sourceHeight)
{
    auto box = layer.destination;
    const auto width = layer.crop.valid() ? layer.crop.width : sourceWidth;
    const auto height = layer.crop.valid() ? layer.crop.height : sourceHeight;
    if (!layer.preserveAspect || width <= 0 || height <= 0) return box;
    const auto scale = std::min(box.width / width, box.height / height);
    box.x += (box.width - width * scale) / 2;
    box.y += (box.height - height * scale) / 2;
    box.width = width * scale;
    box.height = height * scale;
    return box;
}

Destination visibleLayerBounds(const Layer& layer, int sourceWidth, int sourceHeight)
{
    const auto bounds = rotatedBounds(fittedLayerBox(layer, sourceWidth, sourceHeight));
    return {bounds.left, bounds.top, bounds.width(), bounds.height(), 0};
}

void arrangeLayers(SceneTemplate& scene, LayoutPreset preset, const std::vector<std::size_t>& selected, double gap, bool centered)
{
    struct Unit { Bounds bounds; std::vector<std::size_t> members; bool locked{}, selected{}; };
    std::vector<Unit> units;
    std::map<std::string, std::size_t> groups;
    for (std::size_t i = 0; i < scene.layers.size(); ++i) {
        const auto& layer = scene.layers[i];
        std::size_t index = units.size();
        if (!layer.groupId.empty()) index = groups.try_emplace(layer.groupId, index).first->second;
        if (index == units.size()) units.emplace_back();
        auto& unit = units[index];
        unit.members.push_back(i);
        unit.locked = unit.locked || layer.locked;
        unit.selected = unit.selected || selected.empty() || std::find(selected.begin(), selected.end(), i) != selected.end();
        if (layer.visible) unit.bounds.include(rotatedBounds(layer.destination));
    }

    Bounds locked;
    std::vector<const Unit*> movable;
    for (const auto& unit : units) {
        if (unit.bounds.empty() || !unit.selected) continue;
        if (unit.locked) locked.include(unit.bounds);
        else movable.push_back(&unit);
    }
    if (movable.empty()) return;
    const bool column = preset == LayoutPreset::Column;
    const double startX = locked.empty() || column ? canvasMargin : locked.right + gap;
    const double startY = locked.empty() || !column ? canvasMargin : locked.bottom + gap;
    const auto place = [&scene](const Unit& unit, double left, double top) {
        for (auto member : unit.members) {
            auto& d = scene.layers[member].destination;
            d.x += left - unit.bounds.left;
            d.y += top - unit.bounds.top;
        }
    };
    if (preset == LayoutPreset::Hero) {
        place(*movable.front(), startX, startY);
        double top = startY;
        const double left = startX + movable.front()->bounds.width() + gap;
        for (std::size_t i = 1; i < movable.size(); ++i) {
            place(*movable[i], left, top);
            top += movable[i]->bounds.height() + gap;
        }
        return;
    }

    const auto columns = column ? std::size_t{1} : preset == LayoutPreset::Row ? movable.size()
        : std::size_t(std::ceil(std::sqrt(double(movable.size()))));
    const auto rows = (movable.size() + columns - 1) / columns;
    std::vector<double> widths(columns), heights(rows);
    for (std::size_t i = 0; i < movable.size(); ++i) {
        widths[i % columns] = std::max(widths[i % columns], movable[i]->bounds.width());
        heights[i / columns] = std::max(heights[i / columns], movable[i]->bounds.height());
    }
    double top = startY;
    for (std::size_t row = 0; row < rows; ++row) {
        double left = startX;
        for (std::size_t col = 0; col < columns && row * columns + col < movable.size(); ++col) {
            const auto& unit = *movable[row * columns + col];
            place(unit, left + (centered ? (widths[col] - unit.bounds.width()) / 2 : 0),
                        top + (centered ? (heights[row] - unit.bounds.height()) / 2 : 0));
            left += widths[col] + gap;
        }
        top += heights[row] + gap;
    }
}

Error fitCanvasToContent(SceneTemplate& scene)
{
    if (auto error = validateScene(scene)) return error;
    Bounds bounds;
    for (const auto& layer : scene.layers)
        if (layer.visible) bounds.include(rotatedBounds(layer.destination));
    if (bounds.empty()) return {"SCENE_EMPTY", "No visible layers to fit"};

    const auto left = std::floor(snapIntegralEdge(bounds.left)) - canvasMargin;
    const auto top = std::floor(snapIntegralEdge(bounds.top)) - canvasMargin;
    const auto width = std::ceil(snapIntegralEdge(bounds.right)) + canvasMargin - left;
    const auto height = std::ceil(snapIntegralEdge(bounds.bottom)) + canvasMargin - top;
    if (width > 1000000 || height > 1000000 || width * height > 100000000) {
        return {"SCENE_CANVAS_SIZE", std::to_string(std::int64_t(width)) + " × " + std::to_string(std::int64_t(height))};
    }
    auto candidate = scene;
    candidate.width = int(width);
    candidate.height = int(height);
    // Rebase the scene as a whole, including hidden and locked layers.
    for (auto& layer : candidate.layers) {
        layer.destination.x -= left;
        layer.destination.y -= top;
    }
    if (auto error = validateScene(candidate)) return error;
    scene = std::move(candidate);
    return {};
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
