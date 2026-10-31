// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/domain.h"
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QStringList>
#include <map>
class QPainter;
namespace aitsnap {
struct SceneSource { QImage image; std::string artifactId; QString path; std::string sha256; };
using SceneSources = std::map<std::string, SceneSource>;
struct RenderResult { QImage image; bool complete{true}; core::Error error; QStringList warnings; };
QColor sceneColour(const std::string&);
void paintLayerLocal(QPainter&, const core::Layer&, const QImage& image);
RenderResult renderScene(const core::SceneTemplate&, const SceneSources&);
QJsonObject sceneJson(const core::SceneTemplate&);
bool sceneFromJson(const QJsonObject&, core::SceneTemplate&, QString& error);
SceneSources batchSources(const core::CaptureBatch&);
bool loadHistoricalSources(const QString& manifest, SceneSources&, core::CaptureProfile&, QString& error);
} // namespace aitsnap
