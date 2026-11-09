// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "domain.h"
namespace aitsnap::core {
enum class LayoutPreset { Row, Column, Grid, Hero };
enum class Align { Left, Right, Top, Bottom, CentreX, CentreY };
SceneTemplate presetScene(const CaptureProfile&, std::string id, std::string name, int width, int height, LayoutPreset);
void arrangeLayers(SceneTemplate&, LayoutPreset, const std::vector<std::size_t>& selected={}, double gap=12, bool centered=false);
Destination fittedLayerBox(const Layer&, int sourceWidth, int sourceHeight);
Destination visibleLayerBounds(const Layer&, int sourceWidth, int sourceHeight);
Error fitCanvasToContent(SceneTemplate&);
void alignLayers(SceneTemplate&, const std::vector<std::size_t>& selected, Align);
void distributeLayers(SceneTemplate&, const std::vector<std::size_t>& selected, bool horizontal);
} // namespace aitsnap::core
