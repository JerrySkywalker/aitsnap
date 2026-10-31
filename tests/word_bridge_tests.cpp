// SPDX-License-Identifier: Apache-2.0
#include "adapters/report_queue.h"
#include "adapters/word_dispatch.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QThread>
#include <QUuid>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
using namespace aitsnap::word;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static QByteArray hash(const QString& path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "source read"); return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex(); }
static QJsonObject invoke(const QString& helper, const QString& root, const QJsonObject& job, bool recover)
{
    const auto key = QUuid::createUuid().toString(QUuid::WithoutBraces), jobPath = root + "/" + key + ".job.json", resultPath = root + "/" + key + ".result.json"; QString error;
    check(storeReportJob(jobPath, job, error), "native job persistence");
    QStringList args{"--job", jobPath, "--result", resultPath}; if (recover) args.append("--recover");
    QProcess process; process.start(helper, args);
    if (!process.waitForFinished(45000)) { process.kill(); process.waitForFinished(1000); throw std::runtime_error("Native helper timed out; evidence retained"); }
    check(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, "native helper exit");
    QFile file(resultPath); check(file.open(QIODevice::ReadOnly), "native helper result"); return QJsonDocument::fromJson(file.readAll()).object();
}
static QString marker(const QJsonObject& job) { return "AITSnap_" + QString::fromLatin1(QCryptographicHash::hash(job["jobId"].toString().toUtf8(), QCryptographicHash::Sha256).toHex().left(32)); }
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); if (app.arguments().size() != 2) return 2;
    const auto root = QDir(app.arguments()[1]).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    try {
        check(QDir().mkpath(root), "native output root"); QString error;
        CaptureOutcome outcome; outcome.directory = root; outcome.batch.batchId = "native-synthetic-word";
        outcome.batch.request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); outcome.batch.request.context.testId = "PUBLIC-SYNTHETIC";
        for (int i = 0; i < 2; ++i) {
            QImage image(i ? 120 : 1600, i ? 1200 : 400, QImage::Format_RGB32); image.fill(i ? Qt::green : Qt::blue);
            const auto path = root + QString("/synthetic-%1.png").arg(i); check(saveImageNew(image, path, "PNG", error), "native source PNG");
            core::OutputResult output{i ? "COMPOSITE" : "ORIGINAL", path.toStdString(), "SAVED", {}}; output.sha256 = hash(path).toStdString();
            if (i) { output.sceneId = "row"; output.sceneRevision = 1; } else output.regionId = "r0";
            outcome.batch.outputs.push_back(output);
        }
        ReportOptions options; options.documentPath = root + "/synthetic-report.docx";
        const auto helper = QCoreApplication::applicationDirPath() + "/aitsnap-word-bridge.exe";
        ReportQueue queue(root + "/queue", helper);
        check(queue.enqueue(outcome, options, error), "native inline create queued"); options.layout = "table";
        check(queue.enqueue(outcome, options, error), "native table append queued before creation finishes");
        const auto deadline = QDateTime::currentMSecsSinceEpoch() + 130000;
        bool committed = false;
        while (QDateTime::currentMSecsSinceEpoch() < deadline) {
            QCoreApplication::processEvents(); const auto jobs = queue.jobs(); int done = 0;
            for (const auto& job : jobs) { if (job["state"] == "COMMITTED") ++done; check(job["state"] != "UNCERTAIN" && job["state"] != "FAILED", "native queue did not commit; inspect retained job results"); }
            if (done == 2) { committed = true; break; } QThread::msleep(20);
        }
        check(committed, "native two-job deadline");
        auto replay = queue.jobs().front(); replay["state"] = "RUNNING";
        const auto ack = invoke(helper, root, replay, false); check(ack["state"] == "COMMITTED" && ack["recovered"].toBool(), "committed marker acknowledges without duplicate insertion");
        auto prepared = makeReportJob(outcome, options, "append", error); check(!prepared.isEmpty(), "prepared recovery fixture"); prepared["state"] = "UNCERTAIN";
        auto ext = prepared["extensions"].toObject(); ext["dispatchId"] = "synthetic-recovery"; prepared["extensions"] = ext;
        const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); require(hr, "native verification STA");
        {
            auto word = createApplication(); auto documents = word.get(L"Documents").object(); check(documents.get(L"Count").integer() == 0, "preserve unexpected Word documents");
            word.put(L"Visible", Value(false)); word.put(L"DisplayAlerts", Value(0)); word.put(L"AutomationSecurity", Value(3));
            auto document = documents.call(L"Open", {Value(options.documentPath), Value(false), Value(false), Value(false)}).object();
            check(document.get(L"InlineShapes").object().get(L"Count").integer() == 4, "reopen exactly four embedded pictures after replay");
            check(document.get(L"Tables").object().get(L"Count").integer() == 1, "mixed inline/table document");
            const auto text = document.get(L"Content").object().get(L"Text").text();
            check(text.indexOf("Source r0") >= 0 && text.indexOf("Scene row") > text.indexOf("Source r0"), "caption order reopened");
            document.get(L"Variables").object().call(L"Add", {Value(marker(prepared)), Value("PREPARED:" + ext["fingerprint"].toString())}); document.call(L"Save");
            document.call(L"Close", {Value(0)}); document = {}; if (documents.get(L"Count").integer() == 0) word.call(L"Quit", {Value(0)});
        }
        CoUninitialize();
        const auto uncertain = invoke(helper, root, prepared, true); check(uncertain["state"] == "UNCERTAIN", "PREPARED marker cannot authorize blind reinsertion");
        for (const auto& output : outcome.batch.outputs) check(hash(QString::fromStdString(output.path)).toStdString() == output.sha256, "source immutable after Word insertion");
        const QJsonObject receipt{{"status", "PASS"}, {"document", options.documentPath}, {"pictures", 4}, {"tables", 1}, {"replayNoDuplicate", true}, {"preparedRecovery", "UNCERTAIN"}, {"sourcesUnchanged", true}};
        check(saveBytesNew(QJsonDocument(receipt).toJson(), root + "/receipt.json", error), "native receipt");
        std::cout << "WORD_BRIDGE_NATIVE_PASS " << QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString() << '\n';
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << " evidence=" << root.toStdString() << '\n'; return 1; }
}
