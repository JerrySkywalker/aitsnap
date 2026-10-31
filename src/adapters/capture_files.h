// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "json_store.h"
#include "core/capture_service.h"
#include "scene_render.h"
namespace aitsnap {
struct CaptureOutcome {
    core::CaptureBatch batch;
    QString directory;
    double elapsedMs{};
    double renderMs{};
    double captureSaveMs{};
    std::map<std::string, QImage> composites;
    std::vector<core::SceneTemplate> sceneSnapshots;
};
CaptureOutcome captureToFiles(core::ICaptureBackend&, const Settings&, const core::CaptureRequest&, bool save, bool copy = false);
bool writeBatchManifest(const CaptureOutcome&, QString& error);
} // namespace aitsnap
