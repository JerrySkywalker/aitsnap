// SPDX-License-Identifier: Apache-2.0
#include "capture_files.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
namespace aitsnap {
CaptureOutcome captureToFiles(core::ICaptureBackend& backend, const Settings& settings, const core::CaptureRequest& request, bool save)
{
    QElapsedTimer timer; timer.start();
    CaptureOutcome out;
    try { core::CaptureService service(backend); out.batch = service.capture(request); }
    catch (const std::exception& e) {
        out.batch.batchId = request.requestId; out.batch.request = request;
        out.batch.outputs.push_back({"capture", "", "FAILED", {"CAPTURE", e.what()}});
    }
    if (save) {
        const QString batchName = QString::fromStdString(core::safeSegment(request.requestId));
        QDir root(settings.outputDirectory);
        const bool ready = QDir().mkpath(root.absolutePath()) && root.mkdir(batchName);
        if (ready) out.directory = root.absoluteFilePath(batchName);
        for (const auto& c : out.batch.captures) {
            core::OutputResult result{"ORIGINAL", "", "FAILED", c.error};
            result.regionId = c.region.id;
            QString error;
            const QString name = expandFilename(settings, request, c.region, error);
            if (!ready) result.error = {"OUTPUT_DIRECTORY", "Cannot create unique batch directory"};
            else if (!error.isEmpty()) result.error = {"FILENAME", error.toStdString()};
            else if (!c.error) {
                const QString path = QDir(out.directory).filePath(name + "_" + QString::fromStdString(core::safeSegment(c.region.id)) + "." + settings.format.toLower());
                result.path = path.toStdString();
                if (saveImageNew(toQImage(c.image), path, settings.format.toLatin1(), error)) {
                    result.status = "SAVED";
                    QFile file(path);
                    if (file.open(QIODevice::ReadOnly)) result.sha256 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toStdString();
                } else result.error = {"SAVE", error.toStdString()};
            }
            out.batch.outputs.push_back(result);
        }
        if (!ready && out.batch.captures.empty()) out.batch.outputs.push_back({"files", "", "FAILED", {"OUTPUT_DIRECTORY", "Cannot create unique batch directory"}});
    }
    for (auto& frame : out.batch.displayFrames) frame.image = {}; // retain timing/topology, release uncropped desktop buffers
    out.elapsedMs = double(timer.nsecsElapsed()) / 1000000.0;
    return out;
}
bool writeBatchManifest(const CaptureOutcome& out, QString& error)
{
    if (out.directory.isEmpty()) { error = "No saved batch directory"; return false; }
    auto object = batchJson(out.batch);
    auto ext = object["extensions"].toObject(); ext["captureAndSaveMs"] = out.elapsedMs; object["extensions"] = ext;
    return saveBytesNew(QJsonDocument(object).toJson(), QDir(out.directory).filePath("batch.json"), error);
}
} // namespace aitsnap
