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
#include <QUuid>
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
    if(behavior=="daily") {
        const auto target=job["target"].toObject();QFile document(target["documentPath"].toString());
        check(target["mode"]=="create" ? !document.exists() : document.exists(),"daily FIFO create precedes append");
        check(document.open(QIODevice::WriteOnly|QIODevice::Append),"fake daily file");document.write("synthetic-routing-only\n");document.close();
    }
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
    result.batch.request.profile.id = "report-profile"; result.batch.request.profile.name = "Synthetic profile"; result.batch.request.profile.regions.push_back({"r0", "Voltage", {0, 0, 300, 120}, true, 1});
    result.batch.request.issuedAtUtc = "2026-10-31T10:00:00.000Z"; result.batch.outputs.push_back(output); return result;
}
static void behavior(ReportQueue& queue, const QString& directory, const QString& kind)
{
    auto job = queue.jobs().back(); auto ext = job["extensions"].toObject(); ext["testBehavior"] = kind; job["extensions"] = ext; QString error;
    check(storeReportJob(QDir(directory).filePath(job["jobId"].toString() + ".json"), job, error), "set fake behavior");
}
static void nativeDailyProbe(const QString& evidence,const QString& helper)
{
    const auto root=QDir(evidence).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto captures=root+"/截图 空格",batch=captures+"/synthetic-batch",queuePath=root+"/report-jobs";check(QDir().mkpath(batch),"native daily output");
    auto input=fixture(batch);input.batch.request.issuedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();ReportOptions options;options.targetMode="automatic";QString target,error;QList<QJsonObject> jobs;
    for(int step=0;step<2;++step) {
        ReportQueue queue(queuePath,helper);input.batch.batchId=step?"native-daily-second":"native-daily-first";
        check(queue.enqueue(input,options,error),qPrintable("native daily enqueue: "+error));
        const auto current=queue.jobs().back();const auto path=current["target"].toObject()["documentPath"].toString();
        if(step==0){target=path;check(current["target"].toObject()["mode"]=="create","native first create");}
        else check(path==target && current["target"].toObject()["mode"]=="append","native restart same daily append");
        check(await([&]{const auto state=queue.jobs().back()["state"].toString();return state=="COMMITTED" || state=="FAILED" || state=="UNCERTAIN";},90000),"native daily helper completion deadline");
        jobs=queue.jobs();check(jobs.back()["state"]=="COMMITTED",qPrintable("native daily did not commit: "+QString::fromUtf8(QJsonDocument(jobs.back()).toJson(QJsonDocument::Compact))));
    }
    QJsonArray records;for(const auto& job:jobs)records.append(job);
    check(saveBytesNew(QJsonDocument(QJsonObject{{"status","PASS"},{"scope","modern Word synthetic daily create/restart/append; no desktop input or human acceptance"},{"document",target},{"jobs",records}}).toJson(),root+"/receipt.json",error),"native daily receipt");
    std::cout<<"NATIVE_DAILY_REPORT_PASS "<<root.toStdString()<<std::endl;
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        if (app.arguments().contains("--job")) return fakeHelper(app.arguments());
        const int native=app.arguments().indexOf("--native-daily");
        if(native>=0){check(native+2<app.arguments().size(),"native evidence directory and helper required");nativeDailyProbe(app.arguments()[native+1],app.arguments()[native+2]);return 0;}
        QTemporaryDir temporary; check(temporary.isValid(), "temporary report root");
        const auto root = temporary.path(); const auto outcome = fixture(root);
        {
            auto settings=defaultSettings();settings.profiles={outcome.batch.request.profile};const auto legacyPath=root+"/legacy-report-settings.json";QString e;
            settings.report.enabled=true;settings.report.documentPath=root+"/existing.docx";check(saveSettings(legacyPath,settings,e),"legacy report fixture");
            QFile file(legacyPath);check(file.open(QIODevice::ReadOnly),"legacy read");auto object=QJsonDocument::fromJson(file.readAll()).object();file.close();auto report=object["wordReport"].toObject();report.remove("targetMode");object["wordReport"]=report;
            check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"legacy rewrite fixture");const auto original=QJsonDocument(object).toJson();file.write(original);file.close();Settings migrated;
            check(loadSettings(legacyPath,migrated,e) && migrated.report.targetMode=="specified" && migrated.report.enabled && migrated.report.documentPath==settings.report.documentPath,"old explicit target and enabled state preserved");
            check(file.open(QIODevice::ReadOnly) && file.readAll()==original,"loading does not rewrite old settings");file.close();
            report["documentPath"]="";object["wordReport"]=report;check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"legacy blank target fixture");file.write(QJsonDocument(object).toJson());file.close();
            check(loadSettings(legacyPath,migrated,e) && migrated.report.targetMode=="automatic" && migrated.report.enabled,"old blank target uses daily default without changing enabled state");
        }
        const auto dailyRoot=root+"/截图 空间",dailyBatch=dailyRoot+"/batch-one",dailyQueue=root+"/daily-queue";
        check(QDir().mkpath(dailyBatch),"daily capture root");auto daily=fixture(dailyBatch);ReportOptions automatic;automatic.targetMode="automatic";
        const auto when=QDateTime::fromString(QString::fromStdString(daily.batch.request.issuedAtUtc),Qt::ISODateWithMs);QString firstTarget;QByteArray queuedFingerprint;
        {
            ReportQueue queue(dailyQueue,app.applicationFilePath());QString e;
            const auto preview=queue.resolveTarget(automatic,dailyRoot,when,false,e);
            check(!preview.isEmpty() && !QFileInfo::exists(dailyQueue+"/routing/daily-targets-v1.json"),"preview does not reserve or create Word");
            check(saveBytesNew("preexisting-report-must-survive",preview,e),"collision sentinel");
            check(queue.enqueue(daily,automatic,e),"first daily reserve and enqueue");auto first=queue.jobs()[0];firstTarget=first["target"].toObject()["documentPath"].toString();
            check(firstTarget!=preview && QFileInfo(firstTarget).absolutePath()==QDir(dailyRoot).canonicalPath() && first["target"].toObject()["mode"]=="create","daily report directly in screenshot root without overwrite");
            daily.batch.batchId="second-daily-batch";daily.batch.request.issuedAtUtc=when.addSecs(60).toString(Qt::ISODateWithMs).toStdString();
            check(queue.enqueue(daily,automatic,e) && queue.jobs().size()==2,"second batch queues before first DOCX exists");
            check(queue.jobs()[1]["target"].toObject()["documentPath"]==firstTarget && queue.jobs()[1]["target"].toObject()["mode"]=="append","same daily reservation and FIFO mode");
            queuedFingerprint=QJsonDocument(queue.jobs()[0]).toJson();
        }
        {
            ReportQueue queue(dailyQueue,app.applicationFilePath());QString e;
            check(queue.resolveTarget(automatic,dailyRoot,when.addSecs(3600),false,e)==firstTarget && QJsonDocument(queue.jobs()[0]).toJson()==queuedFingerprint,"restart reuses reservation and frozen jobs");
            check(queue.resolveTarget(automatic,dailyRoot,when.addDays(1),false,e)!=firstTarget,"local-day rollover");
            const auto otherRoot=root+"/another-root";check(QDir().mkpath(otherRoot),"other screenshot root");
            check(queue.resolveTarget(automatic,otherRoot,when,true,e)!=firstTarget && QJsonDocument(queue.jobs()[0]).toJson()==queuedFingerprint,"root switch does not retarget queued job");
            for(auto job:queue.jobs()){auto ext=job["extensions"].toObject();ext["testBehavior"]="daily";job["extensions"]=ext;check(storeReportJob(dailyQueue+"/"+job["jobId"].toString()+".json",job,e),"daily fake worker fixture");}
            check(await([&]{const auto jobs=queue.jobs();return jobs.size()==2 && jobs[0]["state"]=="COMMITTED" && jobs[1]["state"]=="COMMITTED";}),"fake daily create and append committed in order");
            check(QFile::rename(firstTarget,firstTarget+".moved"),"simulate moved report");
            check(queue.resolveTarget(automatic,dailyRoot,when,false,e).isEmpty() && e.contains("moved or deleted"),"missing committed report holds without silent replacement");
            ReportOptions specified;specified.documentPath=firstTarget+".moved";check(queue.resolveTarget(specified,dailyRoot,when,false,e).isEmpty(),"specified invalid extension is not replaced by automatic mode");
        }
        ReportOptions options; options.documentPath = root + "/synthetic.docx";
        QString error; auto job = makeReportJob(outcome, options, "create", error); check(!job.isEmpty(), "make schema job");
        check(job["items"].toArray()[0].toObject()["caption"].toString().startsWith("#1 Voltage | ") && !job["extensions"].toObject()["heading"].toString().contains("synthetic-report-batch"), "report names are readable rather than internal ids");
        auto corrupt = job; auto items = corrupt["items"].toArray(); auto item = items[0].toObject(); item["caption"] = "changed"; items[0] = item; corrupt["items"] = items;
        check(!validateReportJob(corrupt, error), "immutable instructions fingerprint"); error.clear();
        const auto serialized = root + "/serialized.json"; check(storeReportJob(serialized, job, error), "persist job"); QJsonObject restored;
        check(readReportJob(serialized, restored, error) && restored == job, "exact queue roundtrip");
        {
            auto legacy = job; auto legacyItems = legacy["items"].toArray(); auto first = legacyItems[0].toObject();
            first["caption"] = "Source r0 | 2026-10-31T10:00:00.000Z"; legacyItems[0] = first; legacy["items"] = legacyItems;
            auto ext = legacy["extensions"].toObject(); ext["heading"] = "AITSnap batch synthetic-report-batch"; legacy["extensions"] = ext;
            ext["fingerprint"] = reportFingerprint(legacy); legacy["extensions"] = ext;
            check(storeReportJob(root + "/legacy.json", legacy, error) && readReportJob(root + "/legacy.json", restored, error) && restored == legacy, "legacy immutable instructions remain exactly readable");
        }
        {
            const auto historyRoot = root + "/history"; check(QDir().mkpath(historyRoot), "history directory");
            const auto historical = fixture(historyRoot); check(writeBatchManifest(historical, error), "saved batch manifest");
            const auto manifestPath = historyRoot + "/batch.json"; QFile manifest(manifestPath);
            check(manifest.open(QIODevice::ReadOnly), "read frozen manifest"); const auto before = manifest.readAll(); manifest.close();
            ReportQueue queue(root + "/history-queue", app.applicationFilePath());
            check(queue.enqueueHistorical(manifestPath, options, error), "report saved batch without recapture");
            check(queue.hasBatchForDocument("synthetic-report-batch", options.documentPath), "batch target association");
            check(!queue.enqueueHistorical(manifestPath, options, error), "same batch target cannot accidentally append twice"); error.clear();
            check(await([&] { return queue.jobs()[0]["state"] == "COMMITTED"; }), "historical job fake helper ack");
            check(queue.jobs()[0]["items"].toArray()[0].toObject()["caption"].toString().startsWith("#1 Voltage | "), "historical name from frozen snapshot");
            check(manifest.open(QIODevice::ReadOnly) && manifest.readAll() == before, "reporting never rewrites saved manifest"); manifest.close();
            QFile source(QString::fromStdString(historical.batch.outputs[0].path)); check(source.open(QIODevice::Append), "tamper fixture only"); source.write("changed"); source.close();
            auto other = options; other.documentPath = root + "/tampered.docx";
            check(!queue.enqueueHistorical(manifestPath, other, error) && error.contains("changed"), "changed historical bytes are rejected before queueing"); error.clear();
            check(queue.jobs().size() == 1, "tampered batch creates no second job");
        }
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
