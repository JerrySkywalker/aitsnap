// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "json_store.h"
#include "core/capture_service.h"
#include "scene_render.h"
namespace aitsnap {
struct AcquiredCapture {
    core::AcquiredDesktop desktop;
    std::vector<core::SceneTemplate> sceneSnapshots;
    QStringList renderIds;
    double frameAcquireMs{};
};

struct CaptureOutcome {
    core::CaptureBatch batch;
    QString directory;
    QString imageDirectory; // Flat mode only; directory retains the batch record location.
    double elapsedMs{};
    double renderMs{};
    double captureSaveMs{};
    double frameAcquireMs{};
    double cropMs{};
    double originalSaveMs{};
    std::map<std::string, QImage> composites;
    std::vector<core::SceneTemplate> sceneSnapshots;
};
AcquiredCapture acquireCapture(core::ICaptureBackend&, const Settings&, const core::CaptureRequest&, bool save, bool copy);
CaptureOutcome outputCapture(AcquiredCapture, const Settings&, bool save);
CaptureOutcome captureToFiles(core::ICaptureBackend&, const Settings&, const core::CaptureRequest&, bool save, bool copy = false);
bool writeBatchManifest(const CaptureOutcome&, QString& error);
} // namespace aitsnap
