// SPDX-License-Identifier: Apache-2.0
#include "local_host.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#include <map>
#include <mutex>
namespace aitsnap {
QJsonObject hostError(const QString& code, const QString& message, const QString& status) { return {{"status", status}, {"error", QJsonObject{{"code", code}, {"message", message}}}}; }
static QString hash(const QByteArray& value) { return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex()); }
struct RequestJournal::Impl {
    QString directory, diagnostic;
    std::unique_ptr<QLockFile> lock;
    std::map<QString, QJsonObject> accepted;
    int capacity;
    std::mutex diagnosticMutex;
    bool ready{true};
    bool write(const QString& path, const QJsonObject& value) {
        QSaveFile file(path); const auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { std::lock_guard<std::mutex> lock(diagnosticMutex); diagnostic = "Cannot persist host receipt"; ready = false; return false; } return true;
    }
};
RequestJournal::RequestJournal(QString directory, int capacity) : impl_(std::make_unique<Impl>())
{
    auto& d = *impl_; d.directory = QDir(directory).absolutePath(); d.capacity = capacity;
    if (!QDir().mkpath(d.directory)) { d.ready = false; d.diagnostic = "Cannot create host receipt store"; return; }
    d.lock = std::make_unique<QLockFile>(d.directory + "/host.lock"); d.lock->setStaleLockTime(0);
    if (!d.lock->tryLock(0)) { d.ready = false; d.diagnostic = "Host receipt store already owned"; return; }
    for (const auto& name : QDir(d.directory).entryList({"*.json"}, QDir::Files, QDir::Name)) {
        QFile file(d.directory + "/" + name); QJsonParseError parse;
        if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) { d.ready = false; d.diagnostic = "Preserved unreadable host receipt"; continue; }
        const auto doc = QJsonDocument::fromJson(file.readAll(), &parse); const auto receipt = doc.object();
        if (parse.error || !doc.isObject() || receipt["schemaVersion"].toInt() != 1 || receipt["idempotencyKey"].toString().isEmpty() || receipt["fingerprint"].toString().size() != 64) { d.ready = false; d.diagnostic = "Preserved invalid host receipt; mutation requires review"; continue; }
        const auto state = receipt["state"].toString(); if (state == "REJECTED") continue;
        const auto key = receipt["idempotencyKey"].toString(); const auto old = d.accepted.find(key);
        if (old != d.accepted.end() && old->second["fingerprint"] != receipt["fingerprint"]) { d.ready = false; d.diagnostic = "Conflicting durable host receipts"; continue; }
        if (old == d.accepted.end() || state == "COMMITTED") d.accepted[key] = receipt;
    }
    if (int(d.accepted.size()) > capacity) { d.ready = false; d.diagnostic = "Host receipt capacity exceeded; no eviction"; }
}
RequestJournal::~RequestJournal() = default;
QString RequestJournal::diagnostic() const { std::lock_guard<std::mutex> lock(impl_->diagnosticMutex); return impl_->diagnostic; }
QJsonObject RequestJournal::dispatch(const QJsonObject& request, const HostHandler& handler)
{
    auto& d = *impl_; const auto correlation = request["correlationId"].toString();
    auto envelope = [&](QJsonObject inner) { inner["schemaVersion"] = 1; inner["correlationId"] = correlation; return inner; };
    const QStringList keys{"schemaVersion", "correlationId", "action", "idempotencyKey", "payload"};
    for (auto i = request.begin(); i != request.end(); ++i) if (!keys.contains(i.key())) return envelope(hostError("UNKNOWN_FIELD", "Unknown host request field"));
    const auto action = request["action"].toString(); const QStringList allowed{"capture", "query-status", "list-profiles", "select-profile", "submit-event", "import-catalog"};
    if (request["schemaVersion"].toDouble() != 1 || correlation.isEmpty() || correlation.size() > 128 || !request["payload"].isObject() || !allowed.contains(action)) return envelope(hostError("PROTOCOL", "Version, correlation, payload or allowlisted action invalid"));
    if (QJsonDocument(request).toJson(QJsonDocument::Compact).size() > 1024 * 1024) return envelope(hostError("REQUEST_SIZE", "Request exceeds 1 MiB"));
    const auto payload = request["payload"].toObject(); const bool mutation = action != "query-status" && action != "list-profiles";
    if (!mutation) { try { return envelope(handler(action, payload, false)); } catch (...) { return envelope(hostError("HANDLER", "Read-only host handler failed")); } }
    const auto key = request["idempotencyKey"].toString(); if (key.isEmpty() || key.size() > 128) return envelope(hostError("IDEMPOTENCY_KEY", "Mutation requires a bounded idempotency key"));
    const auto fingerprint = hash(QJsonDocument(QJsonObject{{"action", action}, {"payload", payload}}).toJson(QJsonDocument::Compact));
    const auto previous = d.accepted.find(key);
    if (previous != d.accepted.end()) {
        const auto& receipt = previous->second;
        if (receipt["fingerprint"] != fingerprint) return envelope(hostError("KEY_CONFLICT", "Accepted key belongs to different immutable instructions"));
        if (receipt["state"] == "COMMITTED") { auto replay = receipt["response"].toObject(); replay["replayed"] = true; return envelope(replay); }
        return envelope(hostError("UNCERTAIN", "Prior intent may have executed; inspect status rather than repeat", "UNCERTAIN"));
    }
    if (!d.ready) return envelope(hostError("JOURNAL_HOLD", d.diagnostic));
    if (int(d.accepted.size()) >= d.capacity) return envelope(hostError("CAPACITY", "Host receipt capacity reached; no silent eviction", "BUSY"));
    QJsonObject preflight; try { preflight = handler(action, payload, true); } catch (...) { return envelope(hostError("PREFLIGHT", "Host preflight failed before execution")); }
    if (preflight["status"] != "READY") return envelope(preflight); // busy never consumes key
    const auto path = d.directory + "/" + hash(key.toUtf8()) + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    QJsonObject receipt{{"schemaVersion", 1}, {"idempotencyKey", key}, {"fingerprint", fingerprint}, {"action", action}, {"createdAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"state", "PENDING"}};
    if (!d.write(path, receipt)) return envelope(hostError("JOURNAL_SAVE", d.diagnostic));
    d.accepted[key] = receipt;
    QJsonObject response; try { response = handler(action, payload, false); } catch (...) { response = hostError("HANDLER_INTERRUPTED", "Execution may have begun", "UNCERTAIN"); }
    const auto status = response["status"].toString(); const bool success = status == "OK" || status == "ACCEPTED" || status == "DUPLICATE";
    const bool uncertain = status == "UNCERTAIN";
    receipt["state"] = success ? "COMMITTED" : uncertain ? "UNCERTAIN" : "REJECTED"; receipt["response"] = response;
    if (!d.write(path, receipt)) return envelope(hostError("UNCERTAIN", "Intent executed but acknowledgement persistence failed", "UNCERTAIN"));
    if (success || uncertain) d.accepted[key] = receipt; else d.accepted.erase(key);
    return envelope(response);
}
} // namespace aitsnap
