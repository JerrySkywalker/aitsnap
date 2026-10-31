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
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static QByteArray hash(const QString& path) { QFile file(path); check(file.open(QIODevice::ReadOnly), "read synthetic evidence"); return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex(); }
struct OwnWord {
    Object app; std::vector<Object> documents; bool owned{};
    OwnWord() { app = createApplication(); check(app.get(L"Documents").object().get(L"Count").integer() == 0, "preserve unexpected Word documents"); owned = true; app.put(L"Visible", Value(false)); app.put(L"DisplayAlerts", Value(0)); app.put(L"AutomationSecurity", Value(3)); }
    Object add() { auto doc = app.get(L"Documents").object().call(L"Add").object(); documents.push_back(doc); return doc; }
    Object open(const QString& path) { auto doc = app.get(L"Documents").object().call(L"Open", {filePath(path), Value(false), Value(false), Value(false)}).object(); documents.push_back(doc); return doc; }
    ~OwnWord() { if (!owned) return; for (auto& doc : documents) { try { doc.call(L"Close", {Value(0)}); } catch (...) {} } try { if (app.get(L"Documents").object().get(L"Count").integer() == 0) app.call(L"Quit", {Value(0)}); } catch (...) {} }
};
static QJsonObject invoke(const CaptureOutcome& outcome, const QString& target, const QString& root)
{
    ReportOptions options; options.documentPath = target; QString error;
    auto job = makeReportJob(outcome, options, "append", error); check(!job.isEmpty(), "make exact-target append job"); job["state"] = "RUNNING";
    auto ext = job["extensions"].toObject(); const auto key = job["jobId"].toString(); ext["dispatchId"] = key; job["extensions"] = ext;
    const auto jobPath = root + "/" + key + ".job.json", resultPath = root + "/" + key + ".result.json";
    check(storeReportJob(jobPath, job, error), "persist native document job");
    QProcess process; process.start(QCoreApplication::applicationDirPath() + "/aitsnap-word-bridge.exe", {"--job", jobPath, "--result", resultPath});
    if (!process.waitForFinished(60000)) { process.kill(); process.waitForFinished(1000); throw std::runtime_error("Word document fixture timed out; retained"); }
    check(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, "native document helper exit");
    QFile file(resultPath); check(file.open(QIODevice::ReadOnly), "read document helper result"); return QJsonDocument::fromJson(file.readAll()).object();
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); if (app.arguments().size() != 3) return 2;
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); if (FAILED(com)) return 3;
    const auto root = QDir(app.arguments()[1]).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    try {
        check(QDir().mkpath(root), "document evidence root"); QString error;
        CaptureOutcome outcome; outcome.directory = root; outcome.batch.batchId = "PUBLIC-M3-DOCUMENTS"; outcome.batch.request.context.testId = "PUBLIC-SYNTHETIC"; outcome.batch.request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
        for (int i = 0; i < 2; ++i) { QImage image(i ? 120 : 1600, i ? 1200 : 400, QImage::Format_RGB32); image.fill(i ? Qt::green : Qt::blue);
            const auto path = root + QString("/source %1.png").arg(i); check(saveImageNew(image, path, "PNG", error), "document synthetic image");
            core::OutputResult output{"ORIGINAL", path.toStdString(), "SAVED", {}}; output.regionId = "r" + std::to_string(i); output.sha256 = hash(path).toStdString(); outcome.batch.outputs.push_back(output);
        }
        const auto largePath = root + "/landscape 510 pages.docx", otherPath = root + "/unrelated synthetic target.docx", protectedPath = root + "/protected synthetic.docx";
        check(QFile::copy(app.arguments()[2], largePath), "copy public long-form fixture without changing its source");
        QString version;
        {
            OwnWord word; version = word.app.get(L"Version").text(); std::cout << "WORD_STAGE create-small-control-documents" << std::endl;
            auto other = word.add(); other.get(L"Content").object().call(L"InsertAfter", {Value(QStringLiteral("PUBLIC OTHER DOCUMENT MUST NOT CHANGE\r"))}); other.call(L"SaveAs2", {filePath(otherPath), Value(12)});
            auto protectedDoc = word.add(); protectedDoc.get(L"Content").object().call(L"InsertAfter", {Value(QStringLiteral("PUBLIC PROTECTED FIXTURE\r"))}); protectedDoc.call(L"Protect", {Value(3)}); protectedDoc.call(L"SaveAs2", {filePath(protectedPath), Value(12)}); protectedDoc.call(L"Close", {Value(0)});
        }
        const auto otherHash = hash(otherPath), protectedHash = hash(protectedPath);
        std::cout << "WORD_STAGE append-large-fixture" << std::endl;
        const auto appended = invoke(outcome, largePath, root); check(appended["state"] == "COMMITTED", "append landscape long-form document");
        const auto refused = invoke(outcome, protectedPath, root); check(refused["state"] == "RETRYABLE", "protected document is pre-mutation failure"); check(hash(protectedPath) == protectedHash, "protected document unchanged");
        int pages{}; bool exactOpen = false, unsavedRefused = false;
        {
            std::cout << "WORD_STAGE reopen-and-page-fit" << std::endl;
            OwnWord word; auto document = word.open(largePath); auto other = word.open(otherPath);
            check(document.get(L"InlineShapes").object().get(L"Count").integer() == 2, "long form embedded pictures reopened");
            document.call(L"Repaginate"); pages = document.call(L"ComputeStatistics", {Value(2), Value(false)}).integer(); check(pages >= 510, "actual long-form page count");
            auto pictures = document.get(L"InlineShapes").object(); auto page = document.get(L"PageSetup").object(); const auto width = page.get(L"PageWidth").number() - page.get(L"LeftMargin").number() - page.get(L"RightMargin").number(), height = page.get(L"PageHeight").number() - page.get(L"TopMargin").number() - page.get(L"BottomMargin").number() - 48;
            for (int i = 1; i <= 2; ++i) { const auto picture = pictures.invoke(L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, {Value(i)}).object(); check(picture.get(L"Width").number() <= width + .1 && picture.get(L"Height").number() <= height + .1, "picture fits landscape page content"); }
            document.call(L"Save"); check(document.get(L"Saved").integer() != 0, "save test pagination before open-target append");
            other.call(L"Activate"); QThread::msleep(1000);
            const auto openAppend = invoke(outcome, largePath, root); check(openAppend["state"] == "COMMITTED", "append exact already-open document while another is active");
            check(document.get(L"InlineShapes").object().get(L"Count").integer() == 4 && other.get(L"InlineShapes").object().get(L"Count").integer() == 0, "only explicit open target receives pictures"); exactOpen = true;
            document.get(L"Content").object().call(L"InsertAfter", {Value(QStringLiteral("PUBLIC UNSAVED USER EDIT\r"))}); const auto before = hash(largePath);
            const auto unsaved = invoke(outcome, largePath, root); check(unsaved["state"] == "RETRYABLE" && hash(largePath) == before, "unsaved target preserved before mutation");
            check(document.get(L"Content").object().get(L"Text").text().contains("PUBLIC UNSAVED USER EDIT"), "in-memory edit preserved"); unsavedRefused = true;
        }
        check(hash(otherPath) == otherHash, "unrelated synthetic document unchanged");
        for (const auto& output : outcome.batch.outputs) check(hash(QString::fromStdString(output.path)).toStdString() == output.sha256, "long-form source hash unchanged");
        const QJsonObject receipt{{"status", "PASS"}, {"wordVersion", version}, {"pages", pages}, {"landscapeFit", true}, {"exactOpenTarget", exactOpen}, {"unsavedPreserved", unsavedRefused}, {"protectedPreserved", true}, {"otherDocumentUnchanged", true}, {"sourcesUnchanged", true}, {"root", root}};
        check(saveBytesNew(QJsonDocument(receipt).toJson(), root + "/receipt.json", error), "document acceptance receipt");
        std::cout << "WORD_DOCUMENTS_NATIVE_PASS " << QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString() << '\n';
        CoUninitialize();
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << " evidence=" << root.toStdString() << '\n'; CoUninitialize(); return 1; }
}
