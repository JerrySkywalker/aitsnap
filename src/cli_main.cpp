// SPDX-License-Identifier: Apache-2.0
#include "adapters/local_host.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <iostream>
using namespace aitsnap;
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); app.setApplicationName("aitsnap-cli"); QCommandLineParser parser; parser.setApplicationDescription("Local AITSnap metadata/capture client; no command executor"); parser.addHelpOption();
    parser.addPositionalArgument("action", "capture | query-status | list-profiles | select-profile | import-catalog | submit-event");
    for (const auto& name : {"pipe-name", "key", "correlation", "mode", "profile-id", "scene-id", "file", "source-system", "event-id"}) parser.addOption(QCommandLineOption(name, QString("Value for %1").arg(name), "value"));
    parser.addOption(QCommandLineOption("capture", "Request capture for a submitted metadata event")); parser.process(app);
    if (parser.positionalArguments().size() != 1) parser.showHelp(2); const auto action = parser.positionalArguments().front();
    QJsonObject payload; QString error; const QStringList actions{"capture", "query-status", "list-profiles", "select-profile", "import-catalog", "submit-event"};
    if (!actions.contains(action)) { std::cerr << "Unsupported action; no generic execution action exists\n"; return 2; }
    if (action == "import-catalog") { if (!parser.isSet("file")) return 2; payload["path"] = QFileInfo(parser.value("file")).absoluteFilePath(); }
    if (action == "submit-event") {
        QFile file(parser.value("file")); if (!parser.isSet("file") || !file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { std::cerr << "Cannot read event within 1 MiB\n"; return 2; }
        QJsonParseError parse; const auto doc = QJsonDocument::fromJson(file.readAll(), &parse); if (parse.error || !doc.isObject()) { std::cerr << "Event must be a JSON object\n"; return 2; } payload["event"] = doc.object(); if (parser.isSet("capture")) payload["capture"] = true;
    }
    if (action == "select-profile") { if (!parser.isSet("profile-id")) return 2; payload["profileId"] = parser.value("profile-id"); }
    if (action == "capture" || action == "submit-event") {
        if (action == "capture" || parser.isSet("mode")) payload["mode"] = parser.isSet("mode") ? parser.value("mode") : "save";
        if (parser.isSet("profile-id")) payload["profileId"] = parser.value("profile-id");
        if (parser.isSet("scene-id")) { QJsonArray scenes; for (const auto& scene : parser.values("scene-id")) scenes.append(scene); payload["sceneIds"] = scenes; }
        if (parser.isSet("source-system")) payload["sourceSystem"] = parser.value("source-system"); if (parser.isSet("event-id")) payload["eventId"] = parser.value("event-id");
    }
    QJsonObject request{{"schemaVersion", 1}, {"correlationId", parser.isSet("correlation") ? parser.value("correlation") : QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"action", action}, {"payload", payload}};
    const bool mutation = action != "query-status" && action != "list-profiles";
    if (mutation) request["idempotencyKey"] = parser.isSet("key") ? parser.value("key") : QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto name = parser.isSet("pipe-name") ? parser.value("pipe-name") : defaultPipeName(error);
    if (name.isEmpty()) { std::cerr << error.toStdString() << '\n'; return 1; }
    auto response = callLocalHost(name, request, error); if (response.isEmpty()) { std::cerr << error.toStdString() << '\n'; return 1; }
    if (mutation) { auto result = response["result"].toObject(); result["clientIdempotencyKey"] = request["idempotencyKey"]; response["result"] = result; }
    std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString() << '\n';
    const auto status = response["status"].toString(); return status == "OK" || status == "ACCEPTED" || status == "DUPLICATE" ? 0 : status == "BUSY" ? 3 : status == "UNCERTAIN" ? 4 : 1;
}
