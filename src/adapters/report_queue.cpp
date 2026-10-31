// SPDX-License-Identifier: Apache-2.0
#include "report_queue.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <set>
namespace aitsnap {
static QString utc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString reportFingerprint(const QJsonObject& job)
{
    const auto ext = job["extensions"].toObject();
    const QJsonObject stable{{"jobId", job["jobId"]}, {"batchId", job["batchId"]}, {"target", job["target"]},
        {"items", job["items"]}, {"sources", ext["sources"]}, {"layout", ext["layout"]}, {"heading", ext["heading"]}};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(stable).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
bool validateReportJob(const QJsonObject& job, QString& error)
{
    const auto target = job["target"].toObject(), ext = job["extensions"].toObject();
    const auto path = target["documentPath"].toString(), mode = target["mode"].toString(), id = job["jobId"].toString();
    const QStringList states{"QUEUED", "RUNNING", "COMMITTED", "RETRYABLE", "FAILED", "UNCERTAIN"};
    if (job["schemaVersion"].toInt() != 1 || id.isEmpty() || id.size() > 100 ||
        id.toStdString() != core::safeSegment(id.toStdString()) || job["batchId"].toString().isEmpty() ||
        !QDateTime::fromString(job["createdAtUtc"].toString(), Qt::ISODateWithMs).isValid() ||
        !states.contains(job["state"].toString()) || job["attemptCount"].toInt(-1) < 0 ||
        !QDir::isAbsolutePath(path) || QFileInfo(path).suffix().compare("docx", Qt::CaseInsensitive) ||
        (mode != "create" && mode != "append") || (ext["layout"] != "inline" && ext["layout"] != "table")) {
        error = "Invalid ReportJob version, identity, state, target or layout"; return false;
    }
    const auto items = job["items"].toArray(), sources = ext["sources"].toArray();
    if (items.isEmpty() || items.size() > 1000 || items.size() != sources.size()) { error = "Report requires 1..1000 ordered source items"; return false; }
    std::set<QString> ids;
    for (int i = 0; i < items.size(); ++i) {
        const auto item = items[i].toObject(), source = sources[i].toObject(); const auto artifact = item["artifactId"].toString();
        const auto sha = source["sha256"].toString();
        if (artifact.isEmpty() || !ids.insert(artifact).second || source["artifactId"] != artifact ||
            !QDir::isAbsolutePath(source["path"].toString()) || sha.size() != 64 ||
            !std::all_of(sha.begin(), sha.end(), [](QChar c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }) ||
            (item["renderAs"] != "inline_image" && item["renderAs"] != "table_cell")) { error = "Invalid/duplicate report source reference"; return false; }
    }
    if (ext["fingerprint"].toString() != reportFingerprint(job)) { error = "Report immutable instructions changed"; return false; }
    return true;
}
bool readReportJob(const QString& path, QJsonObject& job, QString& error)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) { error = "Cannot read bounded report job: " + path; return false; }
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error || !document.isObject()) { error = "Invalid report JSON: " + path; return false; }
    if (!validateReportJob(document.object(), error)) return false; job = document.object(); return true;
}
bool storeReportJob(const QString& path, const QJsonObject& job, QString& error)
{
    if (!validateReportJob(job, error)) return false;
    QSaveFile file(path); const auto bytes = QJsonDocument(job).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}
QJsonObject makeReportJob(const CaptureOutcome& outcome, const ReportOptions& options, const QString& mode, QString& error)
{
    const QFileInfo target(options.documentPath); const auto parent = target.absoluteDir().canonicalPath();
    if (parent.isEmpty() || target.suffix().compare("docx", Qt::CaseInsensitive) ||
        (options.itemMode != "both" && options.itemMode != "originals" && options.itemMode != "composites")) { error = "Select a .docx target in an existing directory and a valid item mode"; return {}; }
    const auto path = target.exists() ? target.canonicalFilePath() : QDir(parent).filePath(target.fileName());
    QJsonArray items, sources;
    for (const auto& output : outcome.batch.outputs) {
        const bool original = output.kind == "ORIGINAL", composite = output.kind == "COMPOSITE";
        if (output.error || output.status != "SAVED" || (!original && !composite) ||
            (original && options.itemMode == "composites") || (composite && options.itemMode == "originals")) continue;
        const auto artifact = QString::fromStdString(outcome.batch.batchId + (original ? ":original:" + output.regionId : ":scene:" + output.sceneId));
        const auto label = original ? "Source " + output.regionId : "Scene " + output.sceneId + " r" + std::to_string(output.sceneRevision);
        items.append(QJsonObject{{"artifactId", artifact}, {"caption", QString::fromStdString(label) + " | " + QString::fromStdString(outcome.batch.request.issuedAtUtc)},
            {"renderAs", options.layout == "table" ? "table_cell" : "inline_image"}});
        sources.append(QJsonObject{{"artifactId", artifact}, {"path", QFileInfo(QString::fromStdString(output.path)).canonicalFilePath()}, {"sha256", QString::fromStdString(output.sha256)}});
    }
    const auto& context = outcome.batch.request.context;
    QJsonObject ext{{"sources", sources}, {"layout", options.layout}, {"batchManifest", QDir(outcome.directory).filePath("batch.json")},
        {"heading", QString::fromStdString("AITSnap batch " + outcome.batch.batchId + " | test=" + context.testId + " device=" + context.deviceId + " station=" + context.stationId + " operator=" + context.operatorId + " run=" + context.sequenceRunId + " step=" + context.stepExecutionId + " command=" + context.commandInvocationId + " event=" + context.eventId)}};
    QJsonObject job{{"schemaVersion", 1}, {"jobId", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"batchId", QString::fromStdString(outcome.batch.batchId)},
        {"createdAtUtc", utc()}, {"updatedAtUtc", utc()}, {"state", "QUEUED"}, {"attemptCount", 0},
        {"target", QJsonObject{{"documentPath", path}, {"mode", mode}}}, {"items", items}, {"extensions", ext}};
    ext["fingerprint"] = reportFingerprint(job); job["extensions"] = ext;
    if (!validateReportJob(job, error)) return {}; return job;
}
ReportQueue::ReportQueue(QString directory, QString helper, QObject* parent, int timeoutMs)
    : QObject(parent), directory_(QDir(directory).absolutePath()), helper_(std::move(helper)), timeoutMs_(timeoutMs)
{
    if (!QDir().mkpath(directory_)) { diagnostic_ = "Cannot create report queue"; return; }
    lock_ = std::make_unique<QLockFile>(QDir(directory_).filePath("queue.lock")); lock_->setStaleLockTime(0);
    if (!lock_->tryLock(0)) { diagnostic_ = "Report queue already owned by another process"; return; }
    ready_ = true;
    for (auto job : jobs()) if (job["state"] == "RUNNING") { job["state"] = "UNCERTAIN"; job["errorCode"] = "RESTART_DURING_RUNNING"; persist(job, QDir(directory_).filePath(job["jobId"].toString() + ".json")); }
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) { finish(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) finish(true); });
    deadline_.setSingleShot(true); connect(&deadline_, &QTimer::timeout, this, [this] { process_.kill(); });
    connect(&tick_, &QTimer::timeout, this, [this] { pump(); }); tick_.start(1000);
}
ReportQueue::~ReportQueue()
{
    tick_.stop(); deadline_.stop(); process_.disconnect(this);
    if (!currentPath_.isEmpty()) { process_.kill(); process_.waitForFinished(1000); current_["state"] = "UNCERTAIN"; current_["errorCode"] = "DESKTOP_EXIT_DURING_RUNNING"; persist(current_, currentPath_); }
}
QList<QJsonObject> ReportQueue::jobs() const
{
    QList<QJsonObject> result;
    for (const auto& name : QDir(directory_).entryList({"*.json"}, QDir::Files, QDir::Name)) {
        if (name.contains(".result.")) continue;
        QJsonObject job; QString error;
        if (readReportJob(QDir(directory_).filePath(name), job, error)) result.append(job);
        else diagnostic_ = "Preserved invalid job " + name + ": " + error;
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        const double left = a["extensions"].toObject()["enqueueSequence"].toDouble(), right = b["extensions"].toObject()["enqueueSequence"].toDouble();
        if (left != right) return left < right;
        if (a["createdAtUtc"] != b["createdAtUtc"]) return a["createdAtUtc"].toString() < b["createdAtUtc"].toString();
        return a["jobId"].toString() < b["jobId"].toString();
    }); return result;
}
bool ReportQueue::persist(QJsonObject& job, const QString& path)
{
    job["updatedAtUtc"] = utc(); QString error;
    if (!storeReportJob(path, job, error)) { diagnostic_ = error; ready_ = false; return false; }
    if (changed) changed(); return true;
}
bool ReportQueue::enqueue(const CaptureOutcome& outcome, const ReportOptions& options, QString& error)
{
    if (!ready_) { error = diagnostic_; return false; }
    QString mode = QFileInfo::exists(options.documentPath) ? "append" : "create";
    const QFileInfo target(options.documentPath);
    const auto canonicalTarget = target.exists() ? target.canonicalFilePath() : QDir(target.absoluteDir().canonicalPath()).filePath(target.fileName());
    double sequence = 1;
    for (const auto& job : jobs()) {
        sequence = std::max(sequence, job["extensions"].toObject()["enqueueSequence"].toDouble() + 1);
        if (job["state"] != "FAILED" && job["state"] != "COMMITTED" && job["target"].toObject()["documentPath"].toString().compare(canonicalTarget, Qt::CaseInsensitive) == 0) mode = "append";
    }
    auto job = makeReportJob(outcome, options, mode, error); if (job.isEmpty()) return false;
    auto ext = job["extensions"].toObject(); ext["enqueueSequence"] = sequence; job["extensions"] = ext;
    const auto path = QDir(directory_).filePath(job["jobId"].toString() + ".json");
    if (QFile::exists(path)) { error = "Report job already exists"; return false; }
    if (!persist(job, path)) { error = diagnostic_; return false; }
    QTimer::singleShot(0, this, [this] { pump(); }); return true;
}
void ReportQueue::pump()
{
    if (!ready_ || !currentPath_.isEmpty()) return;
    const auto all = jobs();
    std::set<QString> pendingTargets;
    for (auto job : all) {
        const auto state = job["state"].toString();
        const auto target = job["target"].toObject()["documentPath"].toString();
        if (std::any_of(all.begin(), all.end(), [&](const auto& other) { return other["state"] == "UNCERTAIN" && other["target"].toObject()["documentPath"].toString().compare(target, Qt::CaseInsensitive) == 0; })) continue;
        if ((state == "QUEUED" || state == "RETRYABLE") && !pendingTargets.insert(target.toCaseFolded()).second) continue;
        const auto next = QDateTime::fromString(job["extensions"].toObject()["nextAttemptUtc"].toString(), Qt::ISODateWithMs);
        if ((state == "QUEUED" || state == "RETRYABLE") && job["attemptCount"].toInt() < 3 && (!next.isValid() || next <= QDateTime::currentDateTimeUtc())) {
            launch(QDir(directory_).filePath(job["jobId"].toString() + ".json"), job, false); return;
        }
    }
}
bool ReportQueue::inspectMarker(const QString& id, QString& error)
{
    if (!ready_ || !currentPath_.isEmpty()) { error = "Report worker is unavailable or busy"; return false; }
    for (auto job : jobs()) if (job["jobId"] == id && job["state"] == "UNCERTAIN") { launch(QDir(directory_).filePath(id + ".json"), job, true); return !currentPath_.isEmpty(); }
    error = "Select an UNCERTAIN job for marker-only inspection"; return false;
}
void ReportQueue::launch(const QString& path, QJsonObject job, bool inspect)
{
    if (!inspect) { job["state"] = "RUNNING"; job["attemptCount"] = job["attemptCount"].toInt() + 1; }
    auto ext = job["extensions"].toObject(); const auto dispatch = QUuid::createUuid().toString(QUuid::WithoutBraces); ext["dispatchId"] = dispatch; job["extensions"] = ext;
    if (!persist(job, path)) return;
    current_ = job; currentPath_ = path; inspecting_ = inspect;
    resultPath_ = QDir(directory_).filePath(job["jobId"].toString() + ".result." + dispatch + ".json");
    QStringList arguments{"--job", path, "--result", resultPath_}; if (inspect) arguments.append("--recover");
    process_.setProgram(helper_); process_.setArguments(arguments); workerClock_.start(); process_.start(); deadline_.start(timeoutMs_);
}
void ReportQueue::finish(bool failedToStart)
{
    if (currentPath_.isEmpty()) return; deadline_.stop();
    QString state = failedToStart && !inspecting_ ? "RETRYABLE" : "UNCERTAIN", code = failedToStart ? "HELPER_UNAVAILABLE" : "HELPER_INTERRUPTED";
    QFile file(resultPath_);
    if (file.open(QIODevice::ReadOnly) && file.size() <= 1024 * 1024) {
        const auto result = QJsonDocument::fromJson(file.readAll()).object();
        const auto candidate = result["state"].toString();
        if (result["jobId"] == current_["jobId"] && result["dispatchId"] == current_["extensions"].toObject()["dispatchId"] &&
            (candidate == "COMMITTED" || candidate == "UNCERTAIN" || (!inspecting_ && (candidate == "RETRYABLE" || candidate == "FAILED")))) {
            state = candidate; code = result["errorCode"].toString();
            if (result.contains("marker")) current_["committedMarker"] = result["marker"];
            auto ext = current_["extensions"].toObject(); ext["lastResult"] = result; current_["extensions"] = ext;
        }
    }
    if (state == "RETRYABLE" && current_["attemptCount"].toInt() >= 3) { state = "FAILED"; code = "RETRY_LIMIT_" + code; }
    current_["state"] = state; if (code.isEmpty()) current_.remove("errorCode"); else current_["errorCode"] = code;
    auto ext = current_["extensions"].toObject(); ext["workerMonotonicMs"] = workerClock_.isValid() ? double(workerClock_.nsecsElapsed()) / 1000000.0 : 0; ext["nextAttemptUtc"] = QDateTime::currentDateTimeUtc().addSecs(5).toString(Qt::ISODateWithMs); current_["extensions"] = ext;
    persist(current_, currentPath_); currentPath_.clear(); current_ = {}; resultPath_.clear();
}
} // namespace aitsnap
