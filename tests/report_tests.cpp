// SPDX-License-Identifier: Apache-2.0
#include "adapters/report_queue.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static bool await(const std::function<bool()>& predicate, int ms = 5000)
{
    const auto until = QDateTime::currentMSecsSinceEpoch() + ms;
    while (!predicate() && QDateTime::currentMSecsSinceEpoch() < until) { QCoreApplication::processEvents(); QThread::msleep(10); }
    return predicate();
}
static int fakeHelper(const QStringList& args)
{
    QJsonObject job; QString error;
    check(readReportJob(args[args.indexOf("--job") + 1], job, error), "fake helper reads exact persisted job");
    const auto ext = job["extensions"].toObject(); const auto behavior = ext["testBehavior"].toString();
    if (behavior == "hang") { QThread::sleep(20); return 1; }
    if (behavior == "crash") return 7;
    const auto state = behavior == "retry" ? "RETRYABLE" : "COMMITTED";
    const QJsonObject result{{"jobId", job["jobId"]}, {"dispatchId", ext["dispatchId"]}, {"state", state}, {"errorCode", behavior == "retry" ? "FAKE_PREMUTATION" : ""}, {"marker", "synthetic-marker"}};
    check(saveBytesNew(QJsonDocument(result).toJson(), args[args.indexOf("--result") + 1], error), "fake result"); return 0;
}
static CaptureOutcome fixture(const QString& directory)
{
    QImage image(300, 120, QImage::Format_RGB32); image.fill(Qt::blue); QString error;
    const auto path = QDir(directory).filePath("synthetic-source.png"); check(saveImageNew(image, path, "PNG", error), "source image");
    QFile file(path); check(file.open(QIODevice::ReadOnly), "source read");
    core::OutputResult output{"ORIGINAL", path.toStdString(), "SAVED", {}}; output.regionId = "r0";
    output.sha256 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toStdString();
    CaptureOutcome result; result.directory = directory; result.batch.batchId = "synthetic-report-batch";
    result.batch.request.issuedAtUtc = "2026-10-31T10:00:00.000Z"; result.batch.outputs.push_back(output); return result;
}
static void behavior(ReportQueue& queue, const QString& directory, const QString& kind)
{
    auto job = queue.jobs().back(); auto ext = job["extensions"].toObject(); ext["testBehavior"] = kind; job["extensions"] = ext; QString error;
    check(storeReportJob(QDir(directory).filePath(job["jobId"].toString() + ".json"), job, error), "set fake behavior");
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        if (app.arguments().contains("--job")) return fakeHelper(app.arguments());
        QTemporaryDir temporary; check(temporary.isValid(), "temporary report root");
        const auto root = temporary.path(); const auto outcome = fixture(root);
        ReportOptions options; options.documentPath = root + "/synthetic.docx";
        QString error; auto job = makeReportJob(outcome, options, "create", error); check(!job.isEmpty(), "make schema job");
        auto corrupt = job; auto items = corrupt["items"].toArray(); auto item = items[0].toObject(); item["caption"] = "changed"; items[0] = item; corrupt["items"] = items;
        check(!validateReportJob(corrupt, error), "immutable instructions fingerprint"); error.clear();
        const auto serialized = root + "/serialized.json"; check(storeReportJob(serialized, job, error), "persist job"); QJsonObject restored;
        check(readReportJob(serialized, restored, error) && restored == job, "exact queue roundtrip");
        {
            const auto queueRoot = root + "/commit"; ReportQueue queue(queueRoot, app.applicationFilePath());
            check(queue.enqueue(outcome, options, error), "enqueue independent of Office");
            check(await([&] { return queue.jobs()[0]["state"] == "COMMITTED"; }), "async committed ack");
            check(queue.jobs()[0]["attemptCount"] == 1, "one attempt");
        }
        for (const auto& kind : {"crash", "hang"}) {
            const auto queueRoot = root + "/" + kind;
            ReportQueue queue(queueRoot, app.applicationFilePath(), nullptr, kind == QString("hang") ? 200 : 5000);
            check(queue.enqueue(outcome, options, error), "enqueue interrupted job"); behavior(queue, queueRoot, kind);
            check(await([&] { return queue.jobs()[0]["state"] == "UNCERTAIN"; }), "crash/timeout remains uncertain");
            const int attempts = queue.jobs()[0]["attemptCount"].toInt(); queue.pump(); check(queue.jobs()[0]["attemptCount"].toInt() == attempts, "uncertain does not auto retry");
        }
        {
            const auto queueRoot = root + "/missing"; ReportQueue queue(queueRoot, root + "/missing-helper.exe");
            check(queue.enqueue(outcome, options, error), "queue survives absent helper");
            for (int attempt = 1; attempt <= 3; ++attempt) {
                check(await([&] { const auto j = queue.jobs()[0]; return j["attemptCount"].toInt() == attempt && j["state"] != "RUNNING"; }), "bounded prelaunch retry");
                auto retry = queue.jobs()[0]; auto ext = retry["extensions"].toObject(); ext.remove("nextAttemptUtc"); retry["extensions"] = ext;
                check(storeReportJob(queueRoot + "/" + retry["jobId"].toString() + ".json", retry, error), "advance test retry clock"); queue.pump();
            }
            check(queue.jobs()[0]["state"] == "FAILED", "retry limit three");
            check(queue.retryFailed(queue.jobs()[0]["jobId"].toString(), error), "explicit retry after unavailable helper limit");
            check(queue.jobs()[0]["state"] == "QUEUED" && queue.jobs()[0]["attemptCount"] == 0, "retry resets bounded dispatch attempts only");
            check(queue.jobs()[0]["extensions"].toObject()["manualRetryHistory"].toArray().size() == 1, "retry preserves audited history");
        }
        {
            const auto queueRoot = root + "/fifo"; ReportQueue queue(queueRoot, app.applicationFilePath());
            check(queue.enqueue(outcome, options, error), "FIFO first create"); behavior(queue, queueRoot, "retry");
            check(queue.enqueue(outcome, options, error), "FIFO second append");
            check(queue.jobs()[0]["target"].toObject()["mode"] == "create" && queue.jobs()[1]["target"].toObject()["mode"] == "append", "same-target create before append");
            check(await([&] { return queue.jobs()[0]["state"] == "RETRYABLE"; }), "first FIFO task waits for retry"); queue.pump();
            check(queue.jobs()[1]["attemptCount"] == 0, "append cannot overtake delayed creation");
            auto independent = options; independent.documentPath = root + "/independent.docx"; check(queue.enqueue(outcome, independent, error), "independent document queue");
            check(await([&] { return queue.jobs()[2]["state"] == "COMMITTED"; }), "other document progresses during retry delay");
        }
        {
            const auto queueRoot = root + "/failed-order"; check(QDir().mkpath(queueRoot), "failed FIFO directory");
            auto failed = makeReportJob(outcome, options, "create", error); auto ext = failed["extensions"].toObject();
            ext["lastResult"] = QJsonObject{{"state", "RETRYABLE"}}; failed["extensions"] = ext; failed["state"] = "FAILED"; failed["attemptCount"] = 3; failed["errorCode"] = "RETRY_LIMIT_FAKE_PREMUTATION";
            const auto id = failed["jobId"].toString(), fingerprint = ext["fingerprint"].toString();
            check(storeReportJob(queueRoot + "/" + id + ".json", failed, error), "persist exhausted pre-mutation failure");
            ReportQueue queue(queueRoot, app.applicationFilePath()); check(queue.enqueue(outcome, options, error), "enqueue after failed create");
            check(queue.jobs()[1]["target"].toObject()["mode"] == "append", "failed creation still precedes append"); queue.pump();
            check(queue.jobs()[1]["attemptCount"] == 0, "failed predecessor prevents later append overtaking");
            auto other = options; other.documentPath = root + "/other-after-failure.docx"; check(queue.enqueue(outcome, other, error), "independent target after failure");
            check(await([&] { return queue.jobs()[2]["state"] == "COMMITTED"; }), "failed target does not block independent document");
            check(queue.retryFailed(id, error), "explicit confirmed pre-mutation retry");
            check(queue.jobs()[0]["extensions"].toObject()["fingerprint"] == fingerprint, "manual retry retains immutable job identity and source instructions");
            check(await([&] { return queue.jobs()[0]["state"] == "COMMITTED" && queue.jobs()[1]["state"] == "COMMITTED"; }), "same-target FIFO progresses after explicit recovery");
            check(!queue.retryFailed(id, error), "committed job cannot be reinserted by retry button"); error.clear();
        }
        {
            const auto queueRoot = root + "/restart"; check(QDir().mkpath(queueRoot), "restart directory");
            job["state"] = "RUNNING"; check(storeReportJob(queueRoot + "/" + job["jobId"].toString() + ".json", job, error), "simulate persisted running");
            ReportQueue queue(queueRoot, app.applicationFilePath()); check(queue.jobs()[0]["state"] == "UNCERTAIN", "restart does not blindly reinsert");
            check(!queue.retryFailed(job["jobId"].toString(), error), "UNCERTAIN cannot use failed retry path"); error.clear();
            check(queue.inspectMarker(job["jobId"].toString(), error), "explicit marker inspection");
            check(await([&] { return queue.jobs()[0]["state"] == "COMMITTED"; }), "inspection can acknowledge committed marker");
        }
        check(QFileInfo::exists(QString::fromStdString(outcome.batch.outputs[0].path)), "original source retained");
        std::cout << "REPORT_QUEUE_PASS persistence fingerprint async ack crash timeout retry3 restart marker-inspection source-retained\n";
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
