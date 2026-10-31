// SPDX-License-Identifier: Apache-2.0
#include "context_store.h"
#include "json_store.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
#include <set>
#include <algorithm>
namespace aitsnap {
static QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
static QString sha(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
static bool fields(const QJsonObject& o, const QStringList& allowed, QString& error) {
    for (auto i = o.begin(); i != o.end(); ++i) if (!allowed.contains(i.key())) { error = "Unsupported field: " + i.key(); return false; } return true;
}
static bool text(const QJsonObject& o, const QStringList& required) { for (const auto& k : required) if (!o[k].isString() || o[k].toString().isEmpty()) return false; return true; }
static bool optionalText(const QJsonObject& o, const QStringList& keys, bool nonempty = true) { for (const auto& k : keys) if (o.contains(k) && (!o[k].isString() || (nonempty && o[k].toString().isEmpty()))) return false; return true; }
static bool integer(const QJsonValue& v, double minimum = 0) { return v.isDouble() && v.toDouble() >= minimum && v.toDouble() <= 9007199254740991.0 && std::floor(v.toDouble()) == v.toDouble(); }
static QJsonObject readObject(const QString& path, QString& error) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly) || f.size() > 8 * 1024 * 1024) { error = "Cannot read bounded JSON: " + path; return {}; }
    QJsonParseError parse; const auto doc = QJsonDocument::fromJson(f.readAll(), &parse); if (parse.error || !doc.isObject()) { error = "Invalid JSON object: " + path; return {}; } return doc.object();
}
static QList<QJsonObject> matchingCatalogs(const QList<QJsonObject>& catalogs, const QString& source, const QString& id, const QString& revision) {
    QList<QJsonObject> result;
    for (const auto& envelope : catalogs) { const auto catalog = envelope["catalog"].toObject(); if (catalog["sourceSystem"] != source) continue;
        for (const auto& value : catalog["sequences"].toArray()) { const auto sequence = value.toObject(); if (sequence["sequenceId"] == id && sequence["revision"] == revision) { result.append(envelope); break; } }
    } return result;
}
bool validateCatalog(const QJsonObject& o, QString& error)
{
    if (!fields(o, {"schemaVersion", "catalogId", "catalogRevision", "sourceSystem", "importedAtUtc", "sourceDigest", "commands", "sequences", "extensions"}, error) ||
        o["schemaVersion"].toDouble() != 1 || !text(o, {"catalogId", "catalogRevision", "sourceSystem", "importedAtUtc", "sourceDigest"}) ||
        !QDateTime::fromString(o["importedAtUtc"].toString(), Qt::ISODateWithMs).isValid() || !o["commands"].isArray() || !o["sequences"].isArray() || (o.contains("extensions") && !o["extensions"].isObject())) { if (error.isEmpty()) error = "Invalid catalog v1 header"; return false; }
    const auto digest = o["sourceDigest"].toString(); if (digest.size() != 64 || !std::all_of(digest.begin(), digest.end(), [](QChar c) { return (c >= '0' && c <= '9') || (c.toLower() >= 'a' && c.toLower() <= 'f'); })) { error = "Catalog requires a SHA-256 declaration"; return false; }
    std::set<QString> commands; std::set<std::pair<QString, QString>> sequences;
    for (const auto& value : o["commands"].toArray()) {
        const auto c = value.toObject();
        if (!fields(c, {"commandId", "name", "description", "parameters"}, error) || !text(c, {"commandId", "name"}) || !commands.insert(c["commandId"].toString()).second) { error = "Invalid/duplicate command definition"; return false; }
        if (!optionalText(c, {"description"}, false) || (c.contains("parameters") && !c["parameters"].isArray())) { error = "Invalid command description/parameter array"; return false; }
        std::set<QString> names;
        for (const auto& parameter : c["parameters"].toArray()) {
            const auto p = parameter.toObject(); const QStringList types{"string", "integer", "number", "boolean", "enum"};
            if (!fields(p, {"name", "type", "unit", "description"}, error) || !text(p, {"name", "type"}) || !types.contains(p["type"].toString()) || !optionalText(p, {"unit"}) || !optionalText(p, {"description"}, false) || !names.insert(p["name"].toString()).second) { error = "Invalid parameter definition"; return false; }
        }
    }
    for (const auto& value : o["sequences"].toArray()) { const auto s = value.toObject();
        if (!fields(s, {"sequenceId", "revision", "name", "steps"}, error) || !text(s, {"sequenceId", "revision", "name"}) || !s["steps"].isArray() || !sequences.emplace(s["sequenceId"].toString(), s["revision"].toString()).second) { error = "Invalid/duplicate sequence revision"; return false; }
    }
    for (const auto& value : o["sequences"].toArray()) {
        std::set<QString> ids;
        for (const auto& step : value.toObject()["steps"].toArray()) {
            const auto s = step.toObject(); const auto kind = s["kind"].toString(); const QStringList kinds{"command", "subsequence", "condition", "loop", "parallel"};
            if (!fields(s, {"stepId", "order", "kind", "commandId", "subsequenceId", "subsequenceRevision", "description", "metadata"}, error) || !text(s, {"stepId", "kind"}) || !integer(s["order"]) || !ids.insert(s["stepId"].toString()).second || !kinds.contains(kind)) { error = "Invalid/duplicate sequence step"; return false; }
            if (kind == "command" && !commands.count(s["commandId"].toString())) { error = "Unresolved command reference"; return false; }
            if (kind == "subsequence" && !sequences.count({s["subsequenceId"].toString(), s["subsequenceRevision"].toString()})) { error = "Unresolved versioned subsequence"; return false; }
            if (!optionalText(s, {"commandId", "subsequenceId", "subsequenceRevision"}) || !optionalText(s, {"description"}, false) || (s.contains("metadata") && !s["metadata"].isObject())) { error = "Invalid step metadata/reference type"; return false; }
        }
    }
    return true;
}
QJsonObject catalogFromCsv(const QByteArray& bytes, QString& error)
{
    // RFC-style quoted fields/newlines, UTF-8, no formula or command evaluation.
    auto input = QString::fromUtf8(bytes); if (input.toUtf8() != bytes) { error = "CSV requires valid UTF-8"; return {}; } if (input.startsWith(QChar(0xfeff))) input.remove(0, 1);
    QList<QStringList> rows; QStringList row; QString cell; bool quoted = false, afterQuote = false;
    for (int i = 0; i < input.size(); ++i) { const auto c = input[i];
        if (quoted) { if (c == '"') { if (i + 1 < input.size() && input[i + 1] == '"') { cell += '"'; ++i; } else { quoted = false; afterQuote = true; } } else cell += c; continue; }
        if (c == '"' && cell.isEmpty() && !afterQuote) { quoted = true; continue; }
        if (c == ',' || c == '\n' || c == '\r') { row.append(cell); cell.clear(); afterQuote = false;
            if (c != ',') { if (c == '\r' && i + 1 < input.size() && input[i + 1] == '\n') ++i; if (row.size() > 1 || !row.front().isEmpty()) rows.append(row); row.clear(); } continue; }
        if (afterQuote || c == '"') { error = "Malformed CSV quote"; return {}; } cell += c;
    }
    if (quoted) { error = "Unterminated CSV quote"; return {}; } if (!cell.isEmpty() || !row.isEmpty() || afterQuote) { row.append(cell); rows.append(row); }
    const QStringList header{"schemaVersion", "catalogId", "catalogRevision", "sourceSystem", "recordType", "commandId", "commandName", "sequenceId", "sequenceRevision", "sequenceName", "stepId", "order", "kind", "subsequenceId", "subsequenceRevision"};
    if (rows.isEmpty() || rows.takeFirst() != header || rows.isEmpty()) { error = "CSV v1 requires the documented exact header and records"; return {}; }
    QJsonObject catalog{{"schemaVersion", 1}, {"catalogId", rows.front().value(1)}, {"catalogRevision", rows.front().value(2)}, {"sourceSystem", rows.front().value(3)}, {"importedAtUtc", now()}, {"sourceDigest", sha(bytes)}};
    QJsonArray commands; QList<QJsonObject> sequences; std::map<std::pair<QString, QString>, int> indices;
    for (const auto& r : rows) {
        if (r.size() != header.size() || r[0] != "1" || r[1] != catalog["catalogId"].toString() || r[2] != catalog["catalogRevision"].toString() || r[3] != catalog["sourceSystem"].toString()) { error = "CSV width/version/catalog identity mismatch"; return {}; }
        if (r[4] == "command") commands.append(QJsonObject{{"commandId", r[5]}, {"name", r[6]}});
        else if (r[4] == "sequence") { const auto key = std::make_pair(r[7], r[8]); if (indices.count(key)) { error = "Duplicate CSV sequence"; return {}; } indices[key] = int(sequences.size()); sequences.append(QJsonObject{{"sequenceId", r[7]}, {"revision", r[8]}, {"name", r[9]}, {"steps", QJsonArray{}}}); }
        else if (r[4] != "step") { error = "Unsupported CSV record type"; return {}; }
    }
    for (const auto& r : rows) if (r[4] == "step") {
        const auto key = std::make_pair(r[7], r[8]); bool ok{}; const auto order = r[11].toInt(&ok);
        if (!indices.count(key) || !ok || order < 0) { error = "CSV step needs a declared sequence and nonnegative order"; return {}; }
        QJsonObject step{{"stepId", r[10]}, {"order", order}, {"kind", r[12]}};
        if (!r[5].isEmpty()) step["commandId"] = r[5]; if (!r[13].isEmpty()) { step["subsequenceId"] = r[13]; step["subsequenceRevision"] = r[14]; }
        auto& sequence = sequences[indices[key]]; auto steps = sequence["steps"].toArray(); steps.append(step); sequence["steps"] = steps;
    }
    QJsonArray array; for (const auto& s : sequences) array.append(s); catalog["commands"] = commands; catalog["sequences"] = array;
    if (!validateCatalog(catalog, error)) return {}; return catalog;
}
ContextStore::ContextStore(QString directory) : directory_(QDir(directory).absolutePath())
{
    if (!QDir().mkpath(directory_ + "/catalogs") || !QDir().mkpath(directory_ + "/events")) { diagnostic_ = "Cannot create context store"; return; }
    for (const auto& name : QDir(directory_ + "/catalogs").entryList({"*.envelope.json"}, QDir::Files, QDir::Name)) {
        QString error; const auto envelope = readObject(directory_ + "/catalogs/" + name, error); const auto digest = name.left(64);
        const auto sourceName = digest + (envelope["format"] == "CSV-v1" ? ".source.csv" : ".source.json"); QFile raw(directory_ + "/catalogs/" + sourceName);
        if (name != digest + ".envelope.json" || envelope["sourceFile"] != sourceName || envelope["measuredSourceDigest"] != digest || envelope["schemaVersion"].toInt() != 1 || !validateCatalog(envelope["catalog"].toObject(), error) || !raw.open(QIODevice::ReadOnly) || raw.size() > 4 * 1024 * 1024 || sha(raw.readAll()) != digest) { diagnostic_ = "Preserved invalid catalog snapshot: " + name; continue; }
        catalogs_.append(envelope);
    }
    for (const auto& name : QDir(directory_ + "/events").entryList({"*.json"}, QDir::Files, QDir::Name)) {
        QString error; const auto envelope = readObject(directory_ + "/events/" + name, error); core::ExecutionEvent event; QJsonObject normalized;
        if (envelope["schemaVersion"].toInt() != 1 || !parseEvent(envelope["event"].toObject(), event, normalized, error, true)) { diagnostic_ = "Preserved invalid event receipt: " + name; continue; }
        event.contentDigest = envelope["sourceContentDigest"].toString().toStdString(); const auto accepted = registry_.ingest(event);
        if (accepted.disposition != core::EventDisposition::Accepted) { diagnostic_ = "Event reload conflict/capacity: " + name; continue; }
        envelopes_.emplace(core::EventKey{event.sourceSystem, event.eventId}, envelope);
    }
}
bool ContextStore::importCatalog(const QString& sourcePath, QString& error)
{
    QFile source(sourcePath); if (!source.open(QIODevice::ReadOnly) || source.size() > 4 * 1024 * 1024) { error = "Cannot read catalog within 4 MiB"; return false; }
    const auto bytes = source.readAll(); const auto digest = sha(bytes); const bool csv = QFileInfo(sourcePath).suffix().compare("csv", Qt::CaseInsensitive) == 0;
    QJsonObject catalog;
    if (csv) catalog = catalogFromCsv(bytes, error); else { QJsonParseError parse; auto document = QJsonDocument::fromJson(bytes, &parse); if (parse.error || !document.isObject()) { error = "Invalid catalog JSON"; return false; } catalog = document.object(); }
    if (!validateCatalog(catalog, error)) return false;
    for (const auto& known : catalogs_) {
        const auto c = known["catalog"].toObject();
        if (c["sourceSystem"] == catalog["sourceSystem"] && c["catalogId"] == catalog["catalogId"] && c["catalogRevision"] == catalog["catalogRevision"]) {
            if (known["measuredSourceDigest"] == digest) return true; error = "Catalog revision already imported with different bytes; use a new source revision"; return false;
        }
    }
    const auto sourceName = digest + (csv ? ".source.csv" : ".source.json");
    const QJsonObject envelope{{"schemaVersion", 1}, {"importer", "AITSnap-normalized-v1"}, {"format", csv ? "CSV-v1" : "JSON-v1"}, {"importedAtUtc", now()},
        {"measuredSourceDigest", digest}, {"sourceReportedDigest", catalog["sourceDigest"]}, {"sourceFile", sourceName}, {"catalog", catalog}};
    const auto rawPath = directory_ + "/catalogs/" + sourceName;
    if (!QFile::exists(rawPath) && !saveBytesNew(bytes, rawPath, error)) return false;
    if (!saveBytesNew(QJsonDocument(envelope).toJson(QJsonDocument::Compact), directory_ + "/catalogs/" + digest + ".envelope.json", error)) return false;
    catalogs_.append(envelope); return true;
}
bool ContextStore::parseEvent(const QJsonObject o, core::ExecutionEvent& event, QJsonObject& normalized, QString& error, bool persisted) const
{
    const auto version = o["schemaVersion"].toDouble();
    if (!fields(o, {"schemaVersion", "eventId", "sourceSystem", "sourceSequenceNo", "sourceTimeUtc", "receivedAtUtc", "eventType", "testContext", "sequence", "command", "correlationStatus", "extensions"}, error) || (version != 1 && version != 2) || !text(o, {"eventId", "sourceSystem", "sourceTimeUtc", "eventType"}) || !integer(o["sourceSequenceNo"]) || !QDateTime::fromString(o["sourceTimeUtc"].toString(), Qt::ISODateWithMs).isValid()) { if (error.isEmpty()) error = "Invalid supported event header/time"; return false; }
    if (version == 1 && !o.contains("sequence")) { error = "Event v1 requires a source sequence; v2 explicitly supports UNLINKED events"; return false; }
    if ((o.contains("receivedAtUtc") && !QDateTime::fromString(o["receivedAtUtc"].toString(), Qt::ISODateWithMs).isValid()) ||
        (o.contains("correlationStatus") && !QStringList{"EXACT", "AMBIGUOUS", "UNLINKED"}.contains(o["correlationStatus"].toString()))) { error = "Invalid supplied receive time/correlation shape"; return false; }
    if (o.contains("extensions") && (!o["extensions"].isObject() || !o["extensions"].toObject().isEmpty())) { error = "Event extensions are not admitted by normalized adapter"; return false; }
    for (const auto& key : {"testContext", "sequence", "command"}) if (o.contains(key) && !o[key].isObject()) { error = "Event context requires objects"; return false; }
    const auto test = o["testContext"].toObject(), sequence = o["sequence"].toObject(), command = o["command"].toObject();
    if (!fields(test, {"testId", "deviceId", "subsystem"}, error) || (o.contains("testContext") && !text(test, {"testId"})) ||
        !fields(sequence, {"definitionId", "definitionRevision", "runId", "stepId", "stepExecutionId", "stepPath"}, error) || (o.contains("sequence") && !text(sequence, {"definitionId", "definitionRevision", "runId"})) ||
        !fields(command, {"definitionId", "invocationId", "attempt", "status", "parameters"}, error) || (o.contains("command") && (!text(command, {"definitionId", "invocationId"}) || !integer(command["attempt"], 1) || command["attempt"].toDouble() > 2147483647))) { if (error.isEmpty()) error = "Incomplete event context object"; return false; }
    if (sequence.contains("stepPath") && !sequence["stepPath"].isArray()) { error = "Step path requires an array"; return false; }
    if (!optionalText(test, {"deviceId", "subsystem"}) || !optionalText(sequence, {"stepId", "stepExecutionId"}) || !optionalText(command, {"status"})) { error = "Invalid optional event identity/status string"; return false; }
    for (const auto& part : sequence["stepPath"].toArray()) if (!part.isString() || part.toString().isEmpty()) { error = "Step path requires nonempty source IDs"; return false; }
    if (command.contains("parameters")) {
        if (!command["parameters"].isObject()) { error = "Parameters require a normalized object"; return false; }
        std::map<QString, QString> allowed;
        for (const auto& envelope : matchingCatalogs(catalogs_, o["sourceSystem"].toString(), sequence["definitionId"].toString(), sequence["definitionRevision"].toString()))
            for (const auto& c : envelope["catalog"].toObject()["commands"].toArray()) if (c.toObject()["commandId"] == command["definitionId"])
                for (const auto& p : c.toObject()["parameters"].toArray()) allowed[p.toObject()["name"].toString()] = p.toObject()["type"].toString();
        const auto parameters = command["parameters"].toObject();
        for (auto i = parameters.begin(); i != parameters.end(); ++i) {
            const auto type = allowed[i.key()]; const auto v = i.value();
            if (type.isEmpty() || ((type == "string" || type == "enum") && !v.isString()) || (type == "integer" && (!v.isDouble() || std::abs(v.toDouble()) > 9007199254740991.0 || std::floor(v.toDouble()) != v.toDouble())) || (type == "number" && !v.isDouble()) || (type == "boolean" && !v.isBool())) { error = "Unknown/untyped command parameter; raw frames are not admitted"; return false; }
        }
    }
    auto canonical = o; canonical.remove("receivedAtUtc"); canonical.remove("correlationStatus");
    event.sourceSystem = o["sourceSystem"].toString().toStdString(); event.eventId = o["eventId"].toString().toStdString(); event.eventType = o["eventType"].toString().toStdString();
    event.sourceTimeUtc = o["sourceTimeUtc"].toString().toStdString(); event.receivedAtUtc = (persisted ? o["receivedAtUtc"].toString() : now()).toStdString(); event.sourceSequenceNo = qint64(o["sourceSequenceNo"].toDouble()); event.contentDigest = sha(QJsonDocument(canonical).toJson(QJsonDocument::Compact)).toStdString();
    event.sequenceDefinitionId = sequence["definitionId"].toString().toStdString(); event.sequenceRevision = sequence["definitionRevision"].toString().toStdString(); event.stepId = sequence["stepId"].toString().toStdString();
    event.commandDefinitionId = command["definitionId"].toString().toStdString(); event.commandStatus = command["status"].toString().toStdString();
    event.context.testId = test["testId"].toString().toStdString(); event.context.deviceId = test["deviceId"].toString().toStdString(); event.context.sequenceRunId = sequence["runId"].toString().toStdString(); event.context.stepExecutionId = sequence["stepExecutionId"].toString().toStdString(); event.context.commandInvocationId = command["invocationId"].toString().toStdString(); event.context.attempt = command["attempt"].toInt();
    for (const auto& part : sequence["stepPath"].toArray()) event.stepPath.push_back(part.toString().toStdString()); event.context = core::correlate(event);
    normalized = o; auto redacted = command; redacted.remove("parameters"); if (o.contains("command")) normalized["command"] = redacted;
    normalized["receivedAtUtc"] = QString::fromStdString(event.receivedAtUtc); normalized["correlationStatus"] = QString::fromStdString(event.context.correlationStatus); return true;
}
core::EventResult ContextStore::preview(QJsonObject input) const
{
    if (QJsonDocument(input).toJson(QJsonDocument::Compact).size() > 1024 * 1024) return {core::EventDisposition::Rejected, {}, {"EVENT_SIZE", "Event exceeds 1 MiB"}};
    core::ExecutionEvent event; QJsonObject normalized; QString error;
    if (!parseEvent(input, event, normalized, error)) return {core::EventDisposition::Rejected, {}, {"EVENT_PARSE", error.toStdString()}};
    auto candidate = registry_; return candidate.ingest(event);
}
core::EventResult ContextStore::ingest(QJsonObject input)
{
    if (QJsonDocument(input).toJson(QJsonDocument::Compact).size() > 1024 * 1024) return {core::EventDisposition::Rejected, {}, {"EVENT_SIZE", "Event exceeds 1 MiB"}};
    core::ExecutionEvent event; QJsonObject normalized; QString error;
    if (!parseEvent(input, event, normalized, error)) return {core::EventDisposition::Rejected, {}, {"EVENT_PARSE", error.toStdString()}};
    auto candidate = registry_; const auto result = candidate.ingest(event); if (result.disposition != core::EventDisposition::Accepted) return result;
    QJsonArray catalogs;
    for (auto catalog : matchingCatalogs(catalogs_, QString::fromStdString(event.sourceSystem), QString::fromStdString(event.sequenceDefinitionId), QString::fromStdString(event.sequenceRevision))) { catalog.remove("sourceFile"); catalogs.append(catalog); }
    const QJsonObject envelope{{"schemaVersion", 1}, {"sourceContentDigest", QString::fromStdString(event.contentDigest)}, {"event", normalized}, {"redaction", "parameter-values-omitted"},
        {"catalogSnapshots", catalogs}, {"definitionStatus", catalogs.size() == 1 ? "RESOLVED" : catalogs.isEmpty() ? "UNRESOLVED" : "MULTIPLE_CATALOGS"}};
    const auto key = sha(QJsonDocument(QJsonArray{QString::fromStdString(event.sourceSystem), QString::fromStdString(event.eventId)}).toJson(QJsonDocument::Compact));
    const auto bytes = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    if (bytes.size() > 8 * 1024 * 1024) return {core::EventDisposition::Rejected, result.context, {"EVENT_SNAPSHOT_SIZE", "Event plus catalog snapshots exceeds 8 MiB"}};
    if (!saveBytesNew(bytes, directory_ + "/events/" + key + ".json", error)) return {core::EventDisposition::Rejected, result.context, {"EVENT_PERSIST", error.toStdString()}};
    registry_ = std::move(candidate); envelopes_.emplace(core::EventKey{event.sourceSystem, event.eventId}, envelope); return result;
}
QList<QJsonObject> ContextStore::events() const { QList<QJsonObject> result; for (const auto& entry : envelopes_) result.append(entry.second); return result; }
core::Context ContextStore::context(const QString& source, const QString& id) const {
    const auto found = registry_.events().find({source.toStdString(), id.toStdString()}); return found == registry_.events().end() ? core::Context{} : found->second.context;
}
static QJsonObject redactCatalog(QJsonObject catalog) {
    catalog.remove("extensions"); QJsonArray commands, sequences;
    for (const auto& v : catalog["commands"].toArray()) { auto c = v.toObject(); c.remove("description"); QJsonArray parameters; for (const auto& p : c["parameters"].toArray()) { auto value = p.toObject(); value.remove("description"); parameters.append(value); } c["parameters"] = parameters; commands.append(c); }
    for (const auto& v : catalog["sequences"].toArray()) { auto s = v.toObject(); QJsonArray steps; for (const auto& v2 : s["steps"].toArray()) { auto step = v2.toObject(); step.remove("description"); step.remove("metadata"); steps.append(step); } s["steps"] = steps; sequences.append(s); }
    catalog["commands"] = commands; catalog["sequences"] = sequences; return catalog;
}
QJsonObject ContextStore::snapshot(const QString& source, const QString& id, bool redacted) const
{
    const auto found = envelopes_.find({source.toStdString(), id.toStdString()}); if (found == envelopes_.end()) return {};
    auto envelope = found->second; QJsonArray catalogs;
    for (const auto& c : envelope["catalogSnapshots"].toArray()) {
        auto copy = c.toObject(); copy.remove("sourceFile"); if (redacted) copy["catalog"] = redactCatalog(copy["catalog"].toObject()); catalogs.append(copy);
    }
    envelope["catalogSnapshots"] = catalogs; return envelope;
}
bool ContextStore::exportMetadata(const QString& path, bool redacted, QString& error) const
{
    QJsonArray array; for (const auto& entry : envelopes_) array.append(snapshot(QString::fromStdString(entry.first.first), QString::fromStdString(entry.first.second), redacted));
    return saveBytesNew(QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"kind", "AITSNAP_CONTEXT_EXPORT"}, {"createdAtUtc", now()}, {"redacted", redacted}, {"events", array}}).toJson(), path, error);
}
} // namespace aitsnap
