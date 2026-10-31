// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "json_store.h"
#include "core/capture_service.h"
namespace aitsnap {
struct CaptureOutcome {
    core::CaptureBatch batch;
    QString directory;
    double elapsedMs{};
};
CaptureOutcome captureToFiles(core::ICaptureBackend&, const Settings&, const core::CaptureRequest&, bool save);
bool writeBatchManifest(const CaptureOutcome&, QString& error);
} // namespace aitsnap
