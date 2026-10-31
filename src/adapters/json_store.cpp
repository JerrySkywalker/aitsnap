// SPDX-License-Identifier: Apache-2.0
#include "json_store.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <algorithm>

namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static QJsonArray rectJson(core::Rect r) { return {r.x, r.y, r.width, r.height}; }
static bool readRect(const QJsonValue& value, core::Rect& r)
{
    const auto a = value.toArray();
    if (a.size() != 4) return false;
    for (const auto& v : a) if (!v.isDouble() || v.toDouble() != double(v.toInt())) return false;
    r = {a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt()}; return r.valid();
}
QString settingsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/settings-v1.json";
}
Settings defaultSettings()
{
    Settings s; s.outputDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/captures";
    return s;
}
QJsonObject profileJson(const core::CaptureProfile& p)
{
    QJsonArray regions, displays;
    for (const auto& r : p.regions) regions.append(QJsonObject{{"id", qs(r.id)}, {"name", qs(r.name)}, {"rect", rectJson(r.rect)}, {"enabled", r.enabled}});
    for (const auto& d : p.calibratedDisplays) displays.append(QJsonObject{{"id", qs(d.id)}, {"rect", rectJson(d.rect)}, {"rotation", d.rotation}});
    return {{"id", qs(p.id)}, {"name", qs(p.name)}, {"revision", p.revision}, {"regions", regions}, {"displays", displays}};
}
bool profileFromJson(const QJsonObject& o, core::CaptureProfile& p, QString& error)
{
    p.id = o["id"].toString().toStdString(); p.name = o["name"].toString().toStdString(); p.revision = o["revision"].toInt();
    p.regions.clear(); p.calibratedDisplays.clear();
    for (const auto& v : o["regions"].toArray()) {
        const auto r = v.toObject(); core::Region region;
        region.id = r["id"].toString().toStdString(); region.name = r["name"].toString().toStdString();
        region.enabled = r["enabled"].toBool(true);
        if (!readRect(r["rect"], region.rect)) { error = "Invalid physical rectangle"; return false; }
        p.regions.push_back(region);
    }
    for (const auto& v : o["displays"].toArray()) {
        const auto d = v.toObject(); core::Display display;
        display.id = d["id"].toString().toStdString(); display.rotation = d["rotation"].toInt();
        if (display.id.empty() || !readRect(d["rect"], display.rect)) { error = "Invalid calibrated display"; return false; }
        p.calibratedDisplays.push_back(display);
    }
    const auto e = core::validateProfile(p);
    if (e) { error = qs(e.message); return false; }
    return true;
}
bool loadSettings(const QString& path, Settings& s, QString& error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { error = f.errorString(); return false; }
    QJsonParseError parse; const auto doc = QJsonDocument::fromJson(f.readAll(), &parse);
    if (parse.error || !doc.isObject()) { error = "Invalid settings JSON; original file preserved"; return false; }
    const auto o = doc.object();
    if (core::validateVersion(o["schemaVersion"].toInt())) { error = "Unsupported settings version; file preserved"; return false; }
    Settings candidate = defaultSettings();
    for (const auto& value : o["profiles"].toArray()) {
        core::CaptureProfile profile;
        if (!profileFromJson(value.toObject(), profile, error)) return false;
        if (std::any_of(candidate.profiles.begin(), candidate.profiles.end(), [&](const auto& p) { return p.id == profile.id; })) { error = "Duplicate profile id"; return false; }
        candidate.profiles.push_back(profile);
    }
    if (candidate.profiles.empty()) { error = "No valid capture profiles"; return false; }
    candidate.activeProfile = o["activeProfile"].toInt();
    if (candidate.activeProfile < 0 || candidate.activeProfile >= int(candidate.profiles.size())) { error = "Invalid selected profile"; return false; }
    candidate.outputDirectory = o["outputDirectory"].toString(candidate.outputDirectory);
    candidate.format = o["format"].toString("PNG");
    if (candidate.format != "PNG" && candidate.format != "JPEG" && candidate.format != "BMP") { error = "Unknown output format"; return false; }
    candidate.filenameTemplate = o["filenameTemplate"].toString(candidate.filenameTemplate);
    candidate.saveHotkey = o["saveHotkey"].toString(candidate.saveHotkey);
    candidate.copyHotkey = o["copyHotkey"].toString(candidate.copyHotkey);
    candidate.bothHotkey = o["bothHotkey"].toString(candidate.bothHotkey);
    candidate.clipboardRegion = o["clipboardRegion"].toString();
    candidate.testId = o["testId"].toString(); candidate.device = o["device"].toString();
    candidate.station = o["station"].toString(); candidate.operatorName = o["operator"].toString();
    candidate.sequence = qint64(o["sequence"].toDouble()); candidate.startInTray = o["startInTray"].toBool();
    s = candidate; return true;
}
bool saveSettings(const QString& path, const Settings& s, QString& error)
{
    QJsonArray profiles;
    for (const auto& p : s.profiles) profiles.append(profileJson(p));
    QJsonObject o{{"schemaVersion", 1}, {"profiles", profiles}, {"activeProfile", s.activeProfile},
        {"outputDirectory", s.outputDirectory}, {"format", s.format}, {"filenameTemplate", s.filenameTemplate},
        {"saveHotkey", s.saveHotkey}, {"copyHotkey", s.copyHotkey}, {"bothHotkey", s.bothHotkey},
        {"clipboardRegion", s.clipboardRegion}, {"testId", s.testId}, {"device", s.device},
        {"station", s.station}, {"operator", s.operatorName}, {"sequence", double(s.sequence)}, {"startInTray", s.startInTray}};
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = "Cannot create settings directory"; return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(o).toJson()) < 0 || !file.commit()) { error = file.errorString(); return false; }
    return true;
}
QImage toQImage(const core::Image& image)
{
    if (!image.valid()) return {};
    return QImage(image.bgra->data(), image.width, image.height, image.width * 4, QImage::Format_ARGB32).copy();
}
QString expandFilename(const Settings& s, const core::CaptureRequest& request, const core::Region& r, QString& error)
{
    QMap<QString, QString> values{{"datetime", QDateTime::fromString(qs(request.issuedAtUtc), Qt::ISODateWithMs).toUTC().toString("yyyyMMdd-HHmmss-zzz")},
        {"sequence", QString::number(s.sequence)}, {"region", qs(r.name)}, {"profile", qs(request.profile.name)},
        {"test", s.testId}, {"device", s.device}, {"station", s.station}, {"operator", s.operatorName},
        {"run", qs(request.context.sequenceRunId)}, {"step", qs(request.context.stepExecutionId)}, {"command", qs(request.context.commandInvocationId)}};
    QString name = s.filenameTemplate;
    QRegularExpression token("\\{([^{}]+)\\}");
    auto matches = token.globalMatch(name);
    while (matches.hasNext()) {
        const auto match = matches.next(); const auto key = match.captured(1);
        if (!values.contains(key)) { error = "Unknown filename token: " + key; return {}; }
        name.replace(match.captured(0), qs(core::safeSegment(values[key].toUtf8().toStdString())));
    }
    if (name.contains('{') || name.contains('}')) { error = "Malformed filename template"; return {}; }
    return qs(core::safeSegment(name.toUtf8().toStdString()));
}
static bool publishTemp(QTemporaryFile& temp, const QString& path, QString& error)
{
    if (!temp.flush()) { error = temp.errorString(); return false; }
    temp.close();
    // QFile::rename refuses an existing destination. Temp remains auto-removed on failure.
    if (!temp.rename(path)) { error = temp.errorString(); return false; }
    temp.setAutoRemove(false); return true;
}
bool saveImageNew(const QImage& image, const QString& path, const QByteArray& format, QString& error)
{
    QTemporaryFile temp(QFileInfo(path).absolutePath() + "/.aitsnap-XXXXXX.tmp");
    if (!temp.open()) { error = temp.errorString(); return false; }
    QImageWriter writer(&temp, format); writer.setQuality(95);
    if (!writer.write(image)) { error = writer.errorString(); return false; }
    return publishTemp(temp, path, error);
}
bool saveBytesNew(const QByteArray& data, const QString& path, QString& error)
{
    QTemporaryFile temp(QFileInfo(path).absolutePath() + "/.aitsnap-XXXXXX.tmp");
    if (!temp.open() || temp.write(data) != data.size()) { error = temp.errorString(); return false; }
    return publishTemp(temp, path, error);
}
QJsonObject batchJson(const core::CaptureBatch& b)
{
    QJsonArray captures, displays, outputs, artifacts;
    int successes = 0, failures = 0;
    for (const auto& c : b.captures) {
        QJsonArray ids; for (const auto& id : c.displayIds) ids.append(qs(id));
        QJsonObject source{{"sourceCaptureId", qs(b.batchId + ":" + c.region.id)}, {"regionId", qs(c.region.id)},
            {"physicalRectPx", rectJson(c.region.rect)}, {"displayIds", ids}, {"outcome", c.error ? "FAILED" : "SUCCEEDED"},
            {"errorCode", c.error ? qs(c.error.code) : QStringLiteral("NONE")}};
        for (const auto& frame : b.displayFrames) {
            if (std::find(c.displayIds.begin(), c.displayIds.end(), frame.display.id) != c.displayIds.end()) { source["acquiredAtUtc"] = qs(frame.acquiredAtUtc); break; }
        }
        captures.append(source);
        if (c.error) ++failures; else ++successes;
    }
    for (const auto& f : b.displayFrames) displays.append(QJsonObject{{"displayId", qs(f.display.id)}, {"acquiredAtUtc", qs(f.acquiredAtUtc)},
        {"startMonotonicNs", QString::number(f.acquisitionStartNs)}, {"endMonotonicNs", QString::number(f.acquisitionEndNs)}, {"errorCode", qs(f.error.code)}});
    for (const auto& o : b.outputs) {
        outputs.append(QJsonObject{{"kind", qs(o.kind)}, {"path", qs(o.path)}, {"status", qs(o.status)},
            {"errorCode", qs(o.error.code)}, {"error", qs(o.error.message)}});
        if (o.error) ++failures;
        if (o.kind == "ORIGINAL") {
            QJsonObject artifact{{"artifactId", qs(b.batchId + ":" + o.regionId)}, {"kind", "ORIGINAL"},
                {"regionId", qs(o.regionId)}, {"saveStatus", qs(o.status)}};
            if (!o.path.empty()) artifact["relativePath"] = QFileInfo(qs(o.path)).fileName();
            if (!o.sha256.empty()) artifact["sha256"] = qs(o.sha256);
            artifacts.append(artifact);
        }
    }
    QJsonObject context;
    const auto& c = b.request.context;
    const std::pair<const char*, std::string> fields[]{{"testId", c.testId}, {"deviceId", c.deviceId},
        {"stationId", c.stationId}, {"operatorId", c.operatorId}, {"sequenceRunId", c.sequenceRunId},
        {"stepExecutionId", c.stepExecutionId}, {"commandInvocationId", c.commandInvocationId}, {"eventId", c.eventId}};
    for (const auto& field : fields) if (!field.second.empty()) context[field.first] = qs(field.second);
    // Runtime v1 manifest extensions preserve full profile snapshot and output diagnostics.
    return {{"schemaVersion", 1}, {"batchId", qs(b.batchId)}, {"requestId", qs(b.request.requestId)},
        {"profileId", qs(b.request.profile.id)}, {"profileRevision", b.request.profile.revision},
        {"createdAtUtc", qs(b.request.issuedAtUtc)}, {"completedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"context", context}, {"captures", captures}, {"artifacts", artifacts}, {"status", successes == 0 ? "FAILED" : failures ? "PARTIAL" : "CAPTURED"},
        {"extensions", QJsonObject{{"profileSnapshot", profileJson(b.request.profile)}, {"displayAcquisitions", displays}, {"outputs", outputs}}}};
}
} // namespace aitsnap
