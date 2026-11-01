// SPDX-License-Identifier: Apache-2.0
#include "scene_render.h"
#include "json_store.h"
#include "desktop/appearance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <stdexcept>

namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
QColor sceneColour(const std::string& value)
{
    const auto text = qs(value); QColor colour(text.left(7));
    if (text.size() == 9) colour.setAlpha(text.mid(7, 2).toInt(nullptr, 16));
    return colour;
}
void paintLayerLocal(QPainter& painter, const core::Layer& layer, const QImage& image)
{
    QRectF crop = layer.crop.valid() ? QRectF(layer.crop.x, layer.crop.y, layer.crop.width, layer.crop.height) : QRectF(image.rect());
    QRectF destination(0, 0, layer.destination.width, layer.destination.height);
    if (layer.preserveAspect && crop.width() > 0 && crop.height() > 0) {
        const auto fitted = crop.size().scaled(destination.size(), Qt::KeepAspectRatio);
        destination = QRectF(QPointF((destination.width() - fitted.width()) / 2, (destination.height() - fitted.height()) / 2), fitted);
    }
    painter.drawImage(destination, image, crop);
}
RenderResult renderScenePreview(core::SceneTemplate scene,const SceneSources& sources,QSize bounds)
{
    if(scene.width<=0 || scene.height<=0 || bounds.isEmpty())return {{},false,{"PREVIEW_SIZE","Invalid preview dimensions"},{}};
    const auto size=QSize(scene.width,scene.height).scaled(bounds,Qt::KeepAspectRatio);const double scale=double(size.width())/scene.width;
    scene.width=std::max(1,size.width());scene.height=std::max(1,size.height());
    for(auto& layer:scene.layers){layer.destination.x*=scale;layer.destination.y*=scale;layer.destination.width*=scale;layer.destination.height*=scale;layer.missingSourcePolicy="placeholder";}
    return renderScene(scene,sources);
}
RenderResult renderScene(const core::SceneTemplate& scene, const SceneSources& sources)
{
    RenderResult result;
    if (auto error = core::validateScene(scene); error) { result.error = error; return result; }
    result.image = QImage(scene.width, scene.height, QImage::Format_ARGB32_Premultiplied);
    if (result.image.isNull()) { result.error = {"SCENE_MEMORY", "Cannot allocate scene canvas"}; return result; }
    result.image.fill(sceneColour(scene.background));
    QPainter painter(&result.image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    auto layers = scene.layers;
    std::sort(layers.begin(), layers.end(), [](const auto& a, const auto& b) { return a.zIndex == b.zIndex ? a.id < b.id : a.zIndex < b.zIndex; });
    int layerNumber=0;
    for (const auto& layer : layers) {
        ++layerNumber;
        if (!layer.visible) continue;
        const auto it = sources.find(layer.sourceRegionId);
        const bool missing = it == sources.end() || it->second.image.isNull() ||
            (!layer.sourceArtifactId.empty() && it->second.artifactId != layer.sourceArtifactId);
        QString missingLabel;
        if (missing) {
            missingLabel=uiText("图层 %1 的来源不可用").arg(layerNumber);
            if (layer.missingSourcePolicy == "fail") { painter.end(); result.image = {}; result.error = {"MISSING_SOURCE",missingLabel.toStdString()}; return result; }
            result.complete = false; result.warnings << missingLabel;
            if (layer.missingSourcePolicy == "skip") continue;
        } else if (layer.crop.valid() && (layer.crop.x < 0 || layer.crop.y < 0 ||
            std::int64_t(layer.crop.x) + layer.crop.width > it->second.image.width() || std::int64_t(layer.crop.y) + layer.crop.height > it->second.image.height())) {
            painter.end(); result.image = {}; result.error = {"SOURCE_CROP", "Crop lies outside retained source pixels"}; return result;
        }
        const auto& d = layer.destination;
        painter.save(); painter.translate(d.x + d.width / 2, d.y + d.height / 2); painter.rotate(std::fmod(d.rotationDeg, 360)); painter.translate(-d.width / 2, -d.height / 2);
        if (missing) { painter.fillRect(QRectF(0, 0, d.width, d.height), QColor("#5c2630")); painter.setPen(Qt::white); painter.drawText(QRectF(0, 0, d.width, d.height), Qt::AlignCenter | Qt::TextWordWrap,missingLabel); }
        else paintLayerLocal(painter, layer, it->second.image);
        painter.restore();
    }
    painter.end(); return result;
}
QJsonObject sceneJson(const core::SceneTemplate& s)
{
    QJsonArray layers;
    for (const auto& l : s.layers) {
        const auto& d = l.destination;
        QJsonObject layer{{"layerId", qs(l.id)}, {"sourceRegionId", qs(l.sourceRegionId)}, {"zIndex", l.zIndex},
            {"visible", l.visible}, {"locked", l.locked}, {"preserveAspect", l.preserveAspect}, {"missingSourcePolicy", qs(l.missingSourcePolicy)},
            {"destination", QJsonObject{{"x", d.x}, {"y", d.y}, {"width", d.width}, {"height", d.height}, {"rotationDeg", d.rotationDeg}}}};
        if (l.crop.valid()) layer["sourceCropPx"] = QJsonArray{l.crop.x, l.crop.y, l.crop.width, l.crop.height};
        if (!l.groupId.empty()) layer["groupId"] = qs(l.groupId);
        if (!l.sourceArtifactId.empty()) layer["sourceArtifactId"] = qs(l.sourceArtifactId);
        layers.append(layer);
    }
    QJsonObject result{{"schemaVersion", 1}, {"sceneId", qs(s.id)}, {"revision", s.revision}, {"name", qs(s.name)},
        {"canvas", QJsonObject{{"widthPx", s.width}, {"heightPx", s.height}, {"background", qs(s.background)}}}, {"layers", layers}};
    if (!s.revisionCreatedAtUtc.empty()) result["revisionCreatedAtUtc"] = qs(s.revisionCreatedAtUtc);
    if (!s.extensionsJson.empty()) {
        QJsonParseError error; const auto extensions = QJsonDocument::fromJson(QByteArray::fromStdString(s.extensionsJson), &error);
        if (error.error || !extensions.isObject()) throw std::invalid_argument("Scene extensionsJson must be an object");
        result["extensions"] = extensions.object();
    }
    return result;
}
static bool allowed(const QJsonObject& o, const QStringList& fields, QString& error)
{
    for (auto it = o.begin(); it != o.end(); ++it) if (!fields.contains(it.key())) { error = "Unknown scene field: " + it.key(); return false; }
    return true;
}
static bool integer(const QJsonValue& value) { return value.isDouble() && value.toDouble() == double(value.toInt()); }
bool sceneFromJson(const QJsonObject& o, core::SceneTemplate& s, QString& error)
{
    if (core::validateVersion(o["schemaVersion"].toInt()) || !allowed(o, {"schemaVersion", "sceneId", "revision", "name", "canvas", "layers", "revisionCreatedAtUtc", "extensions"}, error)) { if (error.isEmpty()) error = "Unsupported scene version"; return false; }
    core::SceneTemplate candidate;
    candidate.id = o["sceneId"].toString().toStdString(); candidate.name = o["name"].toString().toStdString(); candidate.revision = o["revision"].toInt();
    if (o.contains("revisionCreatedAtUtc")) {
        const auto time = o["revisionCreatedAtUtc"].toString();
        if (!QDateTime::fromString(time, Qt::ISODateWithMs).isValid()) { error = "Invalid scene revision UTC time"; return false; }
        candidate.revisionCreatedAtUtc = time.toStdString();
    }
    if (o.contains("extensions")) {
        if (!o["extensions"].isObject()) { error = "Scene extensions must be an object"; return false; }
        candidate.extensionsJson = QJsonDocument(o["extensions"].toObject()).toJson(QJsonDocument::Compact).toStdString();
    }
    const auto canvas = o["canvas"].toObject();
    if (!allowed(canvas, {"widthPx", "heightPx", "background"}, error)) return false;
    if (!integer(o["revision"]) || !integer(canvas["widthPx"]) || !integer(canvas["heightPx"])) { error = "Scene revision/canvas sizes require integers"; return false; }
    candidate.width = canvas["widthPx"].toInt(); candidate.height = canvas["heightPx"].toInt(); candidate.background = canvas["background"].toString().toStdString();
    if (!o["layers"].isArray()) { error = "Scene layers array required"; return false; }
    for (const auto& value : o["layers"].toArray()) {
        const auto object = value.toObject();
        if (!allowed(object, {"layerId", "sourceRegionId", "sourceArtifactId", "zIndex", "visible", "locked", "sourceCropPx", "destination", "missingSourcePolicy", "groupId", "preserveAspect"}, error)) return false;
        if (!integer(object["zIndex"]) || !object["visible"].isBool() || !object["missingSourcePolicy"].isString()) { error = "Layer zIndex, visible and missing-source policy required"; return false; }
        for (const auto& field : {"locked", "preserveAspect"}) if (object.contains(field) && !object[field].isBool()) { error = "Layer boolean required"; return false; }
        for (const auto& field : {"groupId", "sourceArtifactId"}) if (object.contains(field) && (!object[field].isString() || object[field].toString().isEmpty())) { error = "Layer reference string required"; return false; }
        core::Layer layer; layer.id = object["layerId"].toString().toStdString(); layer.sourceRegionId = object["sourceRegionId"].toString().toStdString();
        layer.sourceArtifactId = object["sourceArtifactId"].toString().toStdString(); layer.groupId = object["groupId"].toString().toStdString();
        layer.zIndex = object["zIndex"].toInt(); layer.visible = object["visible"].toBool(true); layer.locked = object["locked"].toBool(); layer.preserveAspect = object["preserveAspect"].toBool(true);
        layer.missingSourcePolicy = object["missingSourcePolicy"].toString("fail").toStdString();
        const auto d = object["destination"].toObject(); if (!allowed(d, {"x", "y", "width", "height", "rotationDeg"}, error)) return false;
        for (const auto& field : {"x", "y", "width", "height"}) if (!d[field].isDouble()) { error = "Layer destination requires numeric geometry"; return false; }
        if (d.contains("rotationDeg") && !d["rotationDeg"].isDouble()) { error = "Layer rotation requires a number"; return false; }
        layer.destination = {d["x"].toDouble(), d["y"].toDouble(), d["width"].toDouble(), d["height"].toDouble(), d["rotationDeg"].toDouble()};
        if (object.contains("sourceCropPx")) {
            const auto crop = object["sourceCropPx"].toArray();
            if (crop.size() != 4) { error = "Crop requires four integer pixels"; return false; }
            for (const auto& n : crop) if (!n.isDouble() || n.toDouble() != double(n.toInt())) { error = "Crop requires integer pixels"; return false; }
            layer.crop = {crop[0].toInt(), crop[1].toInt(), crop[2].toInt(), crop[3].toInt()};
            if (!layer.crop.valid()) { error = "Crop sizes must be positive"; return false; }
        }
        candidate.layers.push_back(layer);
    }
    if (const auto e = core::validateScene(candidate); e) { error = qs(e.message); return false; }
    s = candidate; return true;
}
SceneSources batchSources(const core::CaptureBatch& batch)
{
    SceneSources sources;
    for (const auto& capture : batch.captures) if (!capture.error && capture.image.valid())
        sources.emplace(capture.region.id, SceneSource{toQImage(capture.image), batch.batchId + ":original:" + capture.region.id});
    for (const auto& output : batch.outputs) if (output.kind == "ORIGINAL" && output.status == "SAVED" && sources.count(output.regionId)) {
        sources[output.regionId].path = qs(output.path); sources[output.regionId].sha256 = output.sha256;
    }
    return sources;
}
bool loadHistoricalSources(const QString& manifest, SceneSources& sources, core::CaptureProfile& profile, QString& error)
{
    QFile file(manifest); if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    QJsonParseError parse; const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error || !doc.isObject()) { error = "Invalid batch JSON"; return false; }
    const auto object = doc.object(); if (core::validateVersion(object["schemaVersion"].toInt())) { error = "Unsupported batch version"; return false; }
    if (!profileFromJson(object["extensions"].toObject()["profileSnapshot"].toObject(), profile, error)) return false;
    const auto root = QFileInfo(manifest).absoluteDir().canonicalPath(); SceneSources candidate;
    for (const auto& value : object["artifacts"].toArray()) {
        const auto artifact = value.toObject(); if (artifact["kind"] != "ORIGINAL" || artifact["saveStatus"] != "SAVED") continue;
        const auto relative = QDir::fromNativeSeparators(artifact["relativePath"].toString());
        if (relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.contains(':') || relative.split('/').contains("..")) { error = "Artifact path must remain relative to batch"; return false; }
        const auto path = QDir(root).filePath(relative); const auto canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty() || !canonical.startsWith(root + '/', Qt::CaseInsensitive)) { error = "Artifact missing or outside batch directory"; return false; }
        QFile imageFile(canonical); if (!imageFile.open(QIODevice::ReadOnly)) { error = imageFile.errorString(); return false; }
        if (artifact.contains("sha256")) { QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(&imageFile);
            if (hash.result().toHex() != artifact["sha256"].toString().toLatin1().toLower()) { error = "Original artifact SHA-256 mismatch"; return false; } }
        QImageReader reader(canonical); const auto size = reader.size();
        if (size.width() <= 0 || size.height() <= 0 || qint64(size.width()) * size.height() > 100000000) { error = "Invalid source image dimensions"; return false; }
        auto image = reader.read(); if (image.isNull()) { error = reader.errorString(); return false; }
        const auto regionId = artifact["regionId"].toString().toStdString();
        if (regionId.empty() || candidate.count(regionId)) { error = "Duplicate/missing original region id"; return false; }
        candidate.emplace(regionId, SceneSource{image, artifact["artifactId"].toString().toStdString(), canonical, artifact["sha256"].toString().toStdString()});
    }
    if (candidate.empty()) { error = "No retained source images"; return false; }
    sources = candidate; return true;
}
} // namespace aitsnap
