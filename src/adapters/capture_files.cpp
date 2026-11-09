// SPDX-License-Identifier: Apache-2.0
#include "capture_files.h"
#include "win_capture.h"
#define NOMINMAX
#include <windows.h>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QFileInfo>
#include <algorithm>
namespace aitsnap {
static void digest(core::OutputResult& result)
{
    QFile file(QString::fromStdString(result.path));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file)) { result.error = {"DIGEST_READ", "Saved file could not be hashed"}; return; }
    result.sha256 = hash.result().toHex().toStdString();
}
AcquiredCapture acquireCapture(core::ICaptureBackend& backend, const Settings& settings, const core::CaptureRequest& input, bool save, bool copy)
{
    QElapsedTimer timer; timer.start();
    AcquiredCapture acquired;
    auto request = input;
    request.captureSequence=settings.sequence;
    core::numberRegions(request.profile);
    request.profile.outputMode = settings.outputMode.toStdString(); request.profile.sceneIds.clear();
    for (const auto& id : settings.selectedSceneIds) request.profile.sceneIds.push_back(id.toStdString()); request.profile.clipboardSelection = settings.clipboardRegion.toStdString();
    auto& renderIds = acquired.renderIds;
    if (save && settings.outputMode != "originals") renderIds = settings.selectedSceneIds;
    if (copy && settings.clipboardRegion.startsWith("scene:")) renderIds.append(settings.clipboardRegion.mid(6));
    renderIds.removeDuplicates();
    for (const auto& scene : settings.scenes) if (renderIds.contains(QString::fromStdString(scene.id))) {
        acquired.sceneSnapshots.push_back(scene);
        const auto revision = std::pair<std::string, int>{scene.id, scene.revision};
        if (std::find(request.sceneRevisions.begin(), request.sceneRevisions.end(), revision) == request.sceneRevisions.end()) request.sceneRevisions.push_back(revision);
    }
    try {
        const std::lock_guard<std::mutex> lock(desktopCaptureMutex());
        core::CaptureService service(backend);
        acquired.desktop = service.acquireFrames(request);
    }
    catch (const std::exception& e) {
        auto& batch = acquired.desktop.batch;
        batch.batchId = request.requestId;
        batch.request = request;
        batch.outputs.push_back({"capture", "", "FAILED", {"CAPTURE", e.what()}});
    }
    acquired.frameAcquireMs = double(timer.nsecsElapsed()) / 1000000.0;
    return acquired;
}

CaptureOutcome outputCapture(AcquiredCapture acquired, const Settings& settings, bool save)
{
    QElapsedTimer timer; timer.start();
    CaptureOutcome out;
    out.frameAcquireMs = acquired.frameAcquireMs;
    out.sceneSnapshots = std::move(acquired.sceneSnapshots);
    const auto& renderIds = acquired.renderIds;
    const auto originalRequest = acquired.desktop.batch.request;
    try {
        out.batch = core::CaptureService::cropFrames(std::move(acquired.desktop));
    } catch (const std::exception& e) {
        out.batch.batchId = originalRequest.requestId;
        out.batch.request = originalRequest;
        out.batch.outputs.push_back({"capture", "", "FAILED", {"CAPTURE", e.what()}});
    }
    for (auto& frame : out.batch.displayFrames) frame.image = {}; // retain times/topology, release uncropped desktop buffers
    const auto& request = out.batch.request;
    out.cropMs = double(timer.nsecsElapsed()) / 1000000.0;
    if (save) {
        const QString batchName = QString("%1_%2_%3").arg(
            QDateTime::fromString(QString::fromStdString(request.issuedAtUtc), Qt::ISODateWithMs).toUTC().toString("yyyyMMdd-HHmmss-zzz"),
            QString::number(settings.sequence).rightJustified(4, '0'), QString::fromStdString(core::safeSegment(request.profile.name)));
        const bool flat=settings.imagePathMode=="flat";
        QDir root(settings.outputDirectory);
        if(flat){
            if(QDir().mkpath(root.filePath(".aitsnap/batches"))){
                const auto native=QDir::toNativeSeparators(root.absoluteFilePath(".aitsnap")).toStdWString();const auto flags=GetFileAttributesW(native.c_str());if(flags!=INVALID_FILE_ATTRIBUTES)SetFileAttributesW(native.c_str(),flags|FILE_ATTRIBUTE_HIDDEN);
            }
            out.imageDirectory=root.absolutePath();root=QDir(root.filePath(".aitsnap/batches"));
        }
        bool ready = false;
        if (QDir().mkpath(root.absolutePath())) for (int suffix = 1; suffix <= 10000; ++suffix) {
            const auto candidate = batchName + (suffix == 1 ? QString{} : QString("_%1").arg(suffix));
            if (root.mkdir(candidate)) { ready = true; out.directory = root.absoluteFilePath(candidate); break; }
            if (!root.exists(candidate)) break;
        }
        for (const auto& c : out.batch.captures) {
            core::OutputResult result{"ORIGINAL", "", "FAILED", c.error};
            result.regionId = c.region.id;
            QString error;
            const QString name = expandFilename(settings, request, c.region, error);
            if (!ready) result.error = {"OUTPUT_DIRECTORY", "Cannot create unique batch directory"};
            else if (!error.isEmpty()) result.error = {"FILENAME", error.toStdString()};
            else if (!c.error) {
                const bool directLossless = settings.outputMode != "composites" && settings.format != "JPEG";
                const auto format = directLossless ? settings.format.toLatin1() : QByteArray("PNG");
                const auto prefix = QString::number(c.region.displayNumber).rightJustified(2, '0') + "_";
                const auto evidenceName = prefix + QString::fromStdString(core::safeSegment(c.region.name));
                const auto imageRoot=flat?out.imageDirectory:out.directory;
                const auto directory = directLossless ? imageRoot : QDir(out.directory).filePath("source-evidence");
                QString path;
                if (!directLossless) QDir().mkpath(directory);
                if (saveImageNumbered(toQImage(c.image), directory, directLossless ? name : evidenceName, format, path, error)) { result.status = "SAVED"; }
                else result.error = {"SAVE", error.toStdString()};
                result.path = path.toStdString(); result.relativePath = QDir(imageRoot).relativeFilePath(path).toStdString();
                if (result.status == "SAVED") digest(result);
                if (settings.outputMode != "composites" && settings.format == "JPEG") {
                    core::OutputResult exported{"IMAGE_EXPORT", "", "FAILED", {}}; QString exportPath;
                    if (saveImageNumbered(toQImage(c.image), imageRoot, name, "JPEG", exportPath, error)) exported.status = "SAVED";
                    else exported.error = {"SAVE", error.toStdString()};
                    exported.path = exportPath.toStdString(); exported.relativePath = QDir(imageRoot).relativeFilePath(exportPath).toStdString();
                    if (exported.status == "SAVED") digest(exported);
                    out.batch.outputs.push_back(exported);
                }
            }
            out.batch.outputs.push_back(result);
        }
        if (!ready && out.batch.captures.empty()) out.batch.outputs.push_back({"files", "", "FAILED", {"OUTPUT_DIRECTORY", "Cannot create unique batch directory"}});
    }
    out.originalSaveMs = double(timer.nsecsElapsed()) / 1000000.0 - out.cropMs;
    out.captureSaveMs = out.frameAcquireMs + out.cropMs + out.originalSaveMs;
    if (save && settings.outputMode != "originals" && settings.selectedSceneIds.empty())
        out.batch.outputs.push_back({"scene-render", "", "FAILED", {"NO_SCENES", "Select at least one composition scene"}});
    const auto sources = batchSources(out.batch);
    for (const auto& sceneId : renderIds) {
        core::OutputResult output{"scene-render", "", "FAILED", {}};
        output.sceneId = sceneId.toStdString();
        const auto selected = std::find_if(out.sceneSnapshots.begin(), out.sceneSnapshots.end(), [&](const auto& scene) { return scene.id == output.sceneId; });
        if (selected == out.sceneSnapshots.end()) { output.error = {"SCENE_MISSING", "Selected scene is unavailable"}; out.batch.outputs.push_back(output); continue; }
        output.sceneRevision = selected->revision;
        QElapsedTimer renderClock; renderClock.start(); auto rendered = renderScene(*selected, sources);
        out.renderMs += double(renderClock.nsecsElapsed()) / 1000000.0;
        if (rendered.error) output.error = rendered.error;
        else {
            out.composites.emplace(selected->id, rendered.image); output.status = "RENDERED";
            if (save && settings.outputMode != "originals" && settings.selectedSceneIds.contains(sceneId)) {
                output.kind = "COMPOSITE"; output.status = "FAILED";
                QString error, savedPath;
                const auto imageRoot=out.imageDirectory.isEmpty()?out.directory:out.imageDirectory;
                const auto destination=out.imageDirectory.isEmpty()?QDir(out.directory).filePath("composites"):imageRoot;
                auto stem=QString::fromStdString(core::safeSegment(selected->name)) + QString("_r%1").arg(selected->revision);
                if(!out.imageDirectory.isEmpty()){
                    core::Region sceneRegion;sceneRegion.name=selected->name;
                    stem=expandFilename(settings,request,sceneRegion,error)+QString("_r%1").arg(selected->revision);
                }
                if(!error.isEmpty())output.error={"FILENAME",error.toStdString()};
                else if (out.directory.isEmpty() || !QDir().mkpath(destination)) output.error = {"OUTPUT_DIRECTORY", "Cannot create composite directory"};
                else if (saveImageNumbered(rendered.image, destination, stem, "PNG", savedPath, error)) { output.status = "SAVED"; }
                else output.error = {"SCENE_SAVE", error.toStdString()};
                if (!savedPath.isEmpty()) { output.path = savedPath.toStdString(); output.relativePath = QDir(imageRoot).relativeFilePath(savedPath).toStdString(); }
                if (output.status == "SAVED") digest(output);
            }
            if (!rendered.complete && !output.error) output.error = {"SCENE_INCOMPLETE", rendered.warnings.join("; ").toStdString()};
        }
        out.batch.outputs.push_back(output);
    }
    out.elapsedMs = out.frameAcquireMs + double(timer.nsecsElapsed()) / 1000000.0;
    return out;
}

CaptureOutcome captureToFiles(core::ICaptureBackend& backend, const Settings& settings, const core::CaptureRequest& request, bool save, bool copy)
{
    return outputCapture(acquireCapture(backend, settings, request, save, copy), settings, save);
}

bool writeBatchManifest(const CaptureOutcome& out, QString& error)
{
    if (out.directory.isEmpty()) { error = "No saved batch directory"; return false; }
    auto object = batchJson(out.batch);
    auto ext = object["extensions"].toObject(); ext["captureAndSaveMs"] = out.captureSaveMs; ext["sceneRenderMs"] = out.renderMs; ext["totalCaptureOutputMs"] = out.elapsedMs;
    ext["frameAcquireMs"] = out.frameAcquireMs;
    ext["cropMs"] = out.cropMs;
    ext["originalSaveMs"] = out.originalSaveMs;
    if (out.triggerToFramesMs) ext["triggerToFramesMs"] = *out.triggerToFramesMs;
    if (out.outputQueueMs) ext["outputQueueMs"] = *out.outputQueueMs;
    if(!out.imageDirectory.isEmpty())ext["storageLayout"]="flat";
    QJsonArray scenes; for (const auto& scene : out.sceneSnapshots) scenes.append(sceneJson(scene)); ext["sceneSnapshots"] = scenes; object["extensions"] = ext;
    return saveBytesNew(QJsonDocument(object).toJson(), QDir(out.directory).filePath("batch.json"), error);
}
} // namespace aitsnap
