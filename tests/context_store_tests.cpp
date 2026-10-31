// SPDX-License-Identifier: Apache-2.0
#include "adapters/context_store.h"
#include "adapters/capture_files.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QUuid>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
using namespace aitsnap::core;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static QByteArray read(const QString& path) { QFile file(path); check(file.open(QIODevice::ReadOnly), "read fixture/evidence"); return file.readAll(); }
struct Fake final : ICaptureBackend {
    std::vector<Display> displays() override { return {{"synthetic", {0, 0, 8, 8}, 0}}; }
    DisplayFrame acquire(const Display& d) override { auto bytes = std::make_shared<std::vector<std::uint8_t>>(256, 255); return {d, {8, 8, bytes}, "2026-10-31T10:00:00.000Z", 1, 2, {}}; }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        check(app.arguments().size() >= 2, "fixture directory required"); QTemporaryDir temporary; check(temporary.isValid(), "temporary context root");
        QString root = temporary.path(); if (app.arguments().size() >= 3) { root = QDir(app.arguments()[2]).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); check(QDir().mkpath(root), "evidence root"); }
        const auto fixtures = app.arguments()[1], catalogPath = fixtures + "/sequence-catalog.json"; const auto sourceBytes = read(catalogPath);
        ContextStore store(root + "/store"); QString error;
        check(store.importCatalog(catalogPath, error) && store.importCatalog(catalogPath, error) && store.catalogs().size() == 1, "idempotent read-only catalog import");
        check(read(catalogPath) == sourceBytes && store.catalogs()[0]["measuredSourceDigest"].toString().toLatin1() == QCryptographicHash::hash(sourceBytes, QCryptographicHash::Sha256).toHex(), "measured bytes digest, source unchanged");
        auto catalog = QJsonDocument::fromJson(sourceBytes).object(); auto conflicting = catalog; auto commands = conflicting["commands"].toArray(); auto command = commands[0].toObject(); command["name"] = "Changed"; commands[0] = command; conflicting["commands"] = commands;
        check(saveBytesNew(QJsonDocument(conflicting).toJson(), root + "/conflict.json", error), "conflict input"); error.clear();
        check(!store.importCatalog(root + "/conflict.json", error) && store.catalogs().size() == 1, "immutable catalog revision conflict"); error.clear();
        auto invalid = catalog; auto sequences = invalid["sequences"].toArray(); auto sequence = sequences[0].toObject(); auto steps = sequence["steps"].toArray(); auto step = steps[0].toObject(); step["commandId"] = "missing"; steps[0] = step; sequence["steps"] = steps; sequences[0] = sequence; invalid["sequences"] = sequences;
        check(!validateCatalog(invalid, error), "unresolved catalog command rejected"); error.clear();
        const QStringList header{"schemaVersion", "catalogId", "catalogRevision", "sourceSystem", "recordType", "commandId", "commandName", "sequenceId", "sequenceRevision", "sequenceName", "stepId", "order", "kind", "subsequenceId", "subsequenceRevision"};
        QString csv = header.join(',') + '\n'; auto row = [&](QStringList values) { for (auto& value : values) { value.replace("\"", "\"\""); value = '"' + value + '"'; } csv += values.join(',') + '\n'; };
        auto fields = [&] { QStringList values; for (int i = 0; i < 15; ++i) values.append(""); values[0] = "1"; values[1] = "CAT-CSV"; values[2] = "v1"; values[3] = "SYNTHETIC_AIT_EXECUTOR"; return values; };
        auto values = fields(); values[4] = "command"; values[5] = "CMD-C"; values[6] = "Public, \"quoted\"\nname"; row(values);
        values = fields(); values[4] = "sequence"; values[7] = "SEQ-CSV"; values[8] = "v1"; values[9] = "CSV public sequence"; row(values);
        values = fields(); values[4] = "step"; values[5] = "CMD-C"; values[7] = "SEQ-CSV"; values[8] = "v1"; values[10] = "one"; values[11] = "1"; values[12] = "command"; row(values);
        check(saveBytesNew(csv.toUtf8(), root + "/catalog.csv", error) && store.importCatalog(root + "/catalog.csv", error), "quoted multiline CSV import");
        auto event = QJsonDocument::fromJson(read(fixtures + "/execution-event.json")).object(); const auto accepted = store.ingest(event);
        check(accepted.disposition == EventDisposition::Accepted && accepted.context.correlationStatus == "EXACT" && accepted.context.attempt == 2, "second-attempt ACK exact identity");
        const auto initial = store.snapshot("SYNTHETIC_AIT_EXECUTOR", "EVT-SYN-017"); check(initial["definitionStatus"] == "RESOLVED", "static snapshot resolved");
        auto duplicate = event; duplicate["receivedAtUtc"] = "2030-01-01T00:00:00Z"; duplicate["correlationStatus"] = "UNLINKED";
        check(store.ingest(duplicate).disposition == EventDisposition::Duplicate && store.snapshot("SYNTHETIC_AIT_EXECUTOR", "EVT-SYN-017") == initial, "duplicate preserves first host receipt and snapshot");
        auto otherRun = event; otherRun["eventId"] = "run-one"; auto seq = otherRun["sequence"].toObject(); seq["runId"] = "RUN-SYN-001"; otherRun["sequence"] = seq; otherRun["sourceSequenceNo"] = 1000;
        check(store.ingest(otherRun).disposition == EventDisposition::Accepted, "out-of-order distinct run");
        auto nested = event; nested["eventId"] = "nested-retry"; seq = nested["sequence"].toObject(); seq["stepPath"] = QJsonArray{"parent", "loop-2", "child"}; nested["sequence"] = seq; auto cmd = nested["command"].toObject(); cmd["attempt"] = 3; nested["command"] = cmd; check(store.ingest(nested).disposition == EventDisposition::Accepted, "nested retry retained");
        auto conflict = event; cmd = conflict["command"].toObject(); cmd["parameters"] = QJsonObject{{"channel", "BUS-B"}}; conflict["command"] = cmd;
        check(store.ingest(conflict).disposition == EventDisposition::Conflict, "parameter-content conflict without publishing values");
        auto ambiguous = event; ambiguous["eventId"] = "ambiguous"; seq = ambiguous["sequence"].toObject(); seq.remove("stepExecutionId"); ambiguous["sequence"] = seq; const auto ambiguousResult = store.ingest(ambiguous); check(ambiguousResult.disposition == EventDisposition::Accepted && ambiguousResult.context.correlationStatus == "AMBIGUOUS", "no inferred step execution");
        auto unlinked = event; unlinked["eventId"] = "unlinked"; unlinked.remove("sequence"); cmd = unlinked["command"].toObject(); cmd.remove("parameters"); unlinked["command"] = cmd; check(store.ingest(unlinked).disposition == EventDisposition::Rejected, "v1 keeps mandatory sequence contract"); unlinked["schemaVersion"] = 2; const auto unlinkedResult = store.ingest(unlinked); check(unlinkedResult.disposition == EventDisposition::Accepted && unlinkedResult.context.correlationStatus == "UNLINKED", "v2 has no inferred active run");
        auto forbidden = event; forbidden["execute"] = "raw-command"; check(store.ingest(forbidden).disposition == EventDisposition::Rejected, "no command execution intent");
        forbidden = event; cmd = forbidden["command"].toObject(); cmd["parameters"] = QJsonObject{{"rawFrame", "00ff"}}; forbidden["command"] = cmd; check(store.ingest(forbidden).disposition == EventDisposition::Rejected, "raw frame parameter rejected");
        catalog["catalogRevision"] = "v2"; check(saveBytesNew(QJsonDocument(catalog).toJson(), root + "/catalog-v2.json", error) && store.importCatalog(root + "/catalog-v2.json", error), "new static revision admitted separately");
        check(store.snapshot("SYNTHETIC_AIT_EXECUTOR", "EVT-SYN-017") == initial, "later imports cannot rebind historical event");
        ContextStore restarted(root + "/store"); check(restarted.events().size() == 5 && restarted.ingest(event).disposition == EventDisposition::Duplicate && restarted.snapshot("SYNTHETIC_AIT_EXECUTOR", "EVT-SYN-017") == initial, "restart retains receipt/digest/dedup");
        Fake backend; CaptureProfile profile; profile.id = "p"; profile.name = "Synthetic"; profile.regions = {{"r", "Synthetic region", {0, 0, 4, 4}, true}}; profile.calibratedDisplays = backend.displays();
        Settings settings = defaultSettings(); settings.profiles = {profile}; settings.outputDirectory = root + "/captures";
        CaptureRequest request; request.requestId = "synthetic-context-capture"; request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); request.profile = profile; request.context = accepted.context; request.contextSnapshotJson = QJsonDocument(initial).toJson(QJsonDocument::Compact).toStdString();
        auto capture = captureToFiles(backend, settings, request, true); check(writeBatchManifest(capture, error), "context capture manifest");
        const auto batch = QJsonDocument::fromJson(read(capture.directory + "/batch.json")).object(); check(batch["context"].toObject()["eventId"] == "EVT-SYN-017" && batch["extensions"].toObject()["executionContextSnapshot"] == initial && batch["extensions"].toObject()["contextCorrelationStatus"] == "EXACT", "batch freezes event and catalog provenance");
        check(store.exportMetadata(root + "/redacted-export.json", true, error), "new redacted export");
        check(!read(root + "/redacted-export.json").contains("BUS-A") && !read(capture.directory + "/batch.json").contains("BUS-A"), "parameter values absent from batch/export");
        check(!store.exportMetadata(root + "/redacted-export.json", true, error), "metadata export never overwrites");
        std::cout << "CONTEXT_STORE_PASS JSON/CSV digest immutable-revisions two-runs nested-retry out-of-order duplicate conflict restart redaction batch-link evidence=" << root.toStdString() << '\n';
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
