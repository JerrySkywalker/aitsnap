// SPDX-License-Identifier: Apache-2.0
#include "adapters/capture_files.h"
#include "adapters/batch_paths.h"
#include "adapters/report_queue.h"
#include "core/scene_geometry.h"
#include "desktop/appearance.h"
#include "main_window.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QThread>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using namespace aitsnap;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static QByteArray hash(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"hash input readable");return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex();}
static QJsonObject read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"read fixture");return QJsonDocument::fromJson(f.readAll()).object();}
struct Backend final:core::ICaptureBackend{
    QColor color{Qt::red};
    std::vector<core::Display> displays()override{return {{"synthetic",{0,0,32,32}}};}
    core::DisplayFrame acquire(const core::Display& d)override{
        auto bytes=std::make_shared<std::vector<std::uint8_t>>(32*32*4);
        for(int i=0;i<32*32;++i){
            (*bytes)[i*4]=std::uint8_t(color.blue());
            (*bytes)[i*4+1]=std::uint8_t(color.green());
            (*bytes)[i*4+2]=std::uint8_t(color.red());
            (*bytes)[i*4+3]=255;
        }
        return {d,{32,32,bytes},"2026-11-02T04:00:00.000Z",100,200,{}};
    }
};

static bool await(const std::function<bool()>& condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}

static void captureWhileSaving(const QString& root, Settings settings)
{
    settings.sequence = 0;
    settings.outputDirectory = root + "/captures";
    settings.outputMode = "originals";
    settings.selectedSceneIds.clear();
    settings.livePreviewEnabled = false;
    const auto path = root + "/settings-v1.json";
    QString error;
    check(saveSettings(path, settings, error), "pipeline settings");

    QSemaphore firstOutputGate;
    std::atomic<bool> firstOutputWaiting{false};
    std::atomic<int> acquired{};
    MainWindow::CaptureOperations operations;
    operations.acquire = [&acquired](const auto& snapshot, const auto& request, bool save, bool copy) {
        Backend backend;
        const QColor colors[]{Qt::red, Qt::green, Qt::blue};
        backend.color = colors[std::min(2, acquired.fetch_add(1))];
        return acquireCapture(backend, snapshot, request, save, copy);
    };
    operations.output = [&](AcquiredCapture input, const auto& snapshot, bool save) {
        if (input.desktop.batch.request.captureSequence == 1) {
            firstOutputWaiting = true;
            check(firstOutputGate.tryAcquire(1, 5000), "test did not release first output");
        }
        return outputCapture(std::move(input), snapshot, save);
    };
    std::vector<std::shared_ptr<CaptureOutcome>> completed;
    MainWindow window(nullptr, path, "-", {}, false, [] { return Backend{}.displays(); }, std::move(operations));
    window.captureFinished = [&](auto outcome) { completed.push_back(std::move(outcome)); };
    window.show();
    auto* save = window.findChild<QPushButton*>("captureSave");
    check(save && save->isEnabled(), "normal save button ready");
    save->click();
    check(await([&] { return firstOutputWaiting && window.isVisible() && save->isEnabled(); }, 3000),
          "window and capture action restored while the first output is blocked");
    QTimer::singleShot(2000, &window, [&] { firstOutputGate.release(); });
    save->click();
    check(await([&] { return acquired == 2 && save->isEnabled(); }, 1500) && completed.empty(),
          "second frame acquired while first output is still blocked");
    save->click();
    check(await([&] { return acquired == 3 && save->isEnabled(); }, 1500) && completed.empty(),
          "third frame acquired before the bounded queue fills");
    save->click();
    Settings persisted;
    check(loadSettings(path, persisted, error) && persisted.sequence == 3,
          "queue saturation rejects the fourth request before allocating a sequence");
    check(await([&] { return completed.size() == 3; }, 8000), "all accepted outputs drain in order");
    const QColor expected[]{Qt::red, Qt::green, Qt::blue};
    for (int i = 0; i < 3; ++i) {
        const auto& outcome = *completed[std::size_t(i)];
        check(outcome.batch.request.captureSequence == i + 1, "FIFO completion");
        check(QFileInfo::exists(outcome.directory + "/batch.json"), "completed manifest committed");
        const auto timings = read(outcome.directory + "/batch.json")["extensions"].toObject();
        check(timings["triggerToFramesMs"].toDouble(-1) >= timings["frameAcquireMs"].toDouble() &&
              timings["outputQueueMs"].toDouble(-1) >= 0, "frame and output queue timing is persisted");
        if (i == 1) check(timings["outputQueueMs"].toDouble() >= 1500, "the second frame waited for the blocked first output");
        const auto& file = outcome.batch.outputs.front();
        check(!file.error && file.status == "SAVED" &&
              QImage(QString::fromStdString(file.path)).pixelColor(3, 3) == expected[i],
              "saved batch contains the pixels captured at its own trigger");
    }
    std::cout << "PIPELINE_PASS 2s blocked output, exact next frames, bounded admission, FIFO receipts\n";
}

static void reportDiagnosticPresentation(const QString& root, Settings settings, const CaptureOutcome& batch)
{
    const auto jobDirectory = root + "/report-jobs";
    check(QDir().mkpath(jobDirectory), "diagnostic fixture directory");
    ReportOptions options;
    options.documentPath = root + "/diagnostics.docx";
    QString error;
    auto uncertain = makeReportJob(batch, options, "create", error);
    check(!uncertain.isEmpty(), "diagnostic fixture job");
    uncertain["state"] = "UNCERTAIN";
    uncertain["errorCode"] = "HELPER_TIMEOUT";
    auto details = uncertain["extensions"].toObject();
    details["lastResult"] = QJsonObject{{"state", "UNCERTAIN"}, {"stage", "wait-for-helper"}, {"hresult", "0x80020009"}};
    uncertain["extensions"] = details;
    const auto uncertainId = uncertain["jobId"].toString();
    check(storeReportJob(jobDirectory + "/" + uncertainId + ".json", uncertain, error), "persist uncertain diagnostic");
    auto legacy = makeReportJob(batch, options, "create", error);
    check(!legacy.isEmpty(), "legacy diagnostic fixture job");
    legacy["state"] = "FAILED";
    legacy["errorCode"] = "RETRY_LIMIT_UNKNOWN_PREVIOUS_CODE";
    details = legacy["extensions"].toObject();
    details["lastResult"] = QJsonObject{{"state", "RETRYABLE"}};
    legacy["extensions"] = details;
    const auto legacyId = legacy["jobId"].toString();
    check(storeReportJob(jobDirectory + "/" + legacyId + ".json", legacy, error), "persist legacy diagnostic");

    for (const auto& language : {QString("en-US"), QString("zh-CN")}) {
        installLanguage(language);
        settings.language = language;
        const auto path = root + "/settings-v1.json";
        check(saveSettings(path, settings, error), "diagnostic UI settings");
        MainWindow window(nullptr, path, "-");
        auto* button = window.findChild<QPushButton*>("inspectReportQueue");
        check(button, "report queue dialog entry");
        bool observed = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto* table = dialog->findChild<QTableWidget*>("reportJobs");
            auto* label = dialog->findChild<QLabel*>("reportJobDetails");
            auto* inspect = dialog->findChild<QPushButton*>("inspectReportMarker");
            auto* retry = dialog->findChild<QPushButton*>("retryFailedReport");
            auto* copy = dialog->findChild<QPushButton*>("copyReportDiagnostics");
            if (table && label && inspect && retry && copy) {
                bool timedOut = false, fallback = false;
                for (int row = 0; row < table->rowCount(); ++row) {
                    const auto id = table->item(row, 0)->text();
                    table->setCurrentCell(row, 0);
                    table->selectRow(row);
                    if (id == uncertainId) {
                        copy->click();
                        const auto raw = QApplication::clipboard()->text();
                        timedOut = label->text().contains(language == "en-US" ? QString("did not respond in time") : QString::fromUtf8("超时")) &&
                            inspect->isEnabled() && !retry->isEnabled() && raw.contains(uncertainId) &&
                            raw.contains("HELPER_TIMEOUT") && raw.contains("wait-for-helper") && raw.contains("0x80020009");
                    } else if (id == legacyId) {
                        fallback = label->text().contains(language == "en-US" ? QString("Copy the diagnostics") : QString::fromUtf8("复制诊断详情")) &&
                            !inspect->isEnabled() && retry->isEnabled();
                    }
                }
                observed = timedOut && fallback;
            }
            dialog->reject();
        });
        button->click();
        check(observed, "readable timeout and legacy fallback, safe action gates, copyable diagnostics in both languages");
    }
    installLanguage("en-US");
    std::cout << "WORD_DIAGNOSTIC_UI_PASS bilingual timeout, legacy code, marker/retry gates and copied diagnostics\n";
}

static void firstRunWithoutSampleSources(const QString& root, const Settings& previous)
{
    const auto path = root + "/settings-v1.json";
    QString error;
    MainWindow::CaptureOperations operations;
    operations.acquire = [](const auto& snapshot, const auto& request, bool save, bool copy) {
        Backend backend;
        return acquireCapture(backend, snapshot, request, save, copy);
    };
    {
        MainWindow window(nullptr, path, "-", {}, false, [] { return Backend{}.displays(); }, operations);
        window.show();
        auto* first = window.findChild<QPushButton*>("selectFirstRegion");
        auto* capture = window.findChild<QPushButton*>("captureSave");
        auto* regions = window.findChild<QTableWidget*>("regions");
        check(first && first->isVisible() && capture && !capture->isEnabled() && regions && regions->rowCount() == 0,
              "first launch starts with zero sources and exposes region selection");
        window.findChild<QPushButton*>("addRegion")->click();
        check(regions->rowCount() == 1 && regions->item(0, 4)->text() == "32", "new default source is bounded by active display");
        regions->item(0, 4)->setText("33");
        window.findChild<QPushButton*>("addRegion")->click();
        Settings unchanged;
        check(regions->rowCount() == 1 && loadSettings(path, unchanged, error) &&
              unchanged.profiles.front().regions.front().rect.width == 32,
              "management cannot commit a changed region outside the active display");
        regions->item(0, 4)->setText("16");
        regions->item(0, 5)->setText("16");
        window.findChild<QPushButton*>("applySettings")->click();
        check(regions->rowCount() == 1 && first->isHidden() && capture->isEnabled(), "first valid source enables capture");
        std::shared_ptr<CaptureOutcome> outcome;
        window.captureFinished = [&](auto result) { outcome = std::move(result); };
        capture->click();
        check(await([&] { return bool(outcome); }, 8000) && outcome->batch.captures.size() == 1 &&
              !outcome->batch.captures.front().error && QFileInfo::exists(outcome->directory + "/batch.json"),
              "new first source captures through the normal save entry");
    }
    const auto upgradePath = root + "/upgrade/settings-v1.json";
    check(saveSettings(upgradePath, previous, error), "existing 0.8.3 schema fixture");
    QFile original(upgradePath);
    check(original.open(QIODevice::ReadOnly), "upgrade fixture readable");
    const auto before = original.readAll();
    original.close();
    {
        MainWindow upgraded(nullptr, upgradePath, "-", {}, false, [] { return Backend{}.displays(); });
        check(upgraded.findChild<QTableWidget*>("regions")->rowCount() ==
              int(previous.profiles[std::size_t(previous.activeProfile)].regions.size()), "existing sources survive startup");
        check(upgraded.findChild<QPushButton*>("selectFirstRegion")->isHidden(), "populated profile omits first-source prompt");
    }
    check(original.open(QIODevice::ReadOnly) && original.readAll() == before, "startup preserves existing settings bytes");
    std::cout << "FIRST_RUN_PASS zero sources, first source saves a batch, existing settings retained\n";
}

static void reportRemovalPresentation(const QString& root, Settings settings, const CaptureOutcome& batch)
{
    const auto jobs = root + "/report-jobs";
    check(QDir().mkpath(jobs), "removal UI fixture directory");
    QString error;
    QStringList ids;
    const auto seed = [&](const char* state, const QString& document) {
        ReportOptions options; options.documentPath = document;
        auto job = makeReportJob(batch, options, "append", error);
        check(!job.isEmpty(), "removal UI job");
        job["state"] = state;
        if (job["state"] == "COMMITTED") job["committedMarker"] = "receipt";
        if (job["state"] == "QUEUED") {
            auto ext = job["extensions"].toObject();
            ext["nextAttemptUtc"] = QDateTime::currentDateTimeUtc().addDays(1).toString(Qt::ISODateWithMs);
            job["extensions"] = ext;
        }
        const auto id = job["jobId"].toString();
        ids.append(id);
        check(storeReportJob(jobs + "/" + id + ".json", job, error), "persist removal UI job");
    };
    seed("QUEUED", root + "/waiting.docx");
    seed("UNCERTAIN", root + "/uncertain.docx");
    seed("COMMITTED", root + "/committed.docx");
    const auto path = root + "/settings-v1.json";
    check(saveSettings(path, settings, error), "removal UI settings");
    MainWindow window(nullptr, path, "-");
    bool verified = false;
    QTimer::singleShot(0, &window, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* table = dialog->findChild<QTableWidget*>("reportJobs");
        auto* remove = dialog->findChild<QPushButton*>("removeReportJobs");
        auto* clear = dialog->findChild<QPushButton*>("clearReportQueue");
        auto* history = dialog->findChild<QCheckBox*>("showRemovedReportJobs");
        if (table && remove && clear && history && table->rowCount() == 3) {
            const auto accept = [&] {
                QTimer::singleShot(0, dialog, [] {
                    auto* prompt = QApplication::activeModalWidget();
                    if (prompt && prompt->objectName() == "confirmReportRemoval")
                        prompt->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Yes)->click();
                });
            };
            for (int row = 0; row < table->rowCount(); ++row) {
                if (table->item(row, 0)->text() != ids[0]) continue;
                table->setCurrentCell(row, 0);
                table->selectRow(row);
                break;
            }
            if (remove->isEnabled()) {
                accept();
                remove->click();
                if (table->rowCount() == 2 && clear->isEnabled()) {
                    accept();
                    clear->click();
                    if (table->rowCount() == 1 && table->item(0, 0)->text() == ids[1]) {
                        history->click();
                        verified = table->rowCount() == 3;
                    }
                }
            }
        }
        dialog->reject();
    });
    window.findChild<QPushButton*>("inspectReportQueue")->click();
    check(verified, "operator can remove one, clear others, and review removed tasks");
    check(read(jobs + "/" + ids[0] + ".json")["state"] == "CANCELLED" &&
          read(jobs + "/" + ids[1] + ".json")["state"] == "UNCERTAIN" &&
          read(jobs + "/" + ids[2] + ".json")["extensions"].toObject().contains("archivedAtUtc"),
          "UI operations persist cancellation and archive and retain uncertain task");
    std::cout << "WORD_REMOVAL_UI_PASS selected removal, clear, protection and history toggle\n";
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
    QTemporaryDir temp;check(temp.isValid(),"temporary root");QString error;Backend backend;auto settings=defaultSettings();settings.hotkeysEnabled={false,false,false};settings.imagePathMode="flat";settings.filenameTemplate="same name";
    core::CaptureProfile profile;profile.id="p";profile.name="Profile";profile.regions={{"r","Voltage",{0,0,16,16},true,1}};profile.calibratedDisplays=backend.displays();settings.profiles={profile};settings.scenes={core::presetScene(profile,"s","Scene",160,90,core::LayoutPreset::Grid)};settings.selectedSceneIds=QStringList{"s"};settings.outputMode="both";
    core::CaptureRequest request;request.profile=profile;request.requestId="batch-one";request.issuedAtUtc="2026-11-02T04:00:00.000Z";
    for(const auto& format:{"PNG","BMP","JPEG"}){
        settings.outputDirectory=temp.filePath(format);settings.format=format;settings.sequence=1;auto output=captureToFiles(backend,settings,request,true,true);check(writeBatchManifest(output,error),"flat manifest saved");const auto manifest=QDir(output.directory).filePath("batch.json");const auto object=read(manifest);
        check(object["extensions"].toObject()["storageLayout"]=="flat" && batchImageRoot(manifest,object)==QDir(settings.outputDirectory).canonicalPath(),"fixed layout resolves correct root");
        const auto files=QDir(settings.outputDirectory).entryList({"*.png","*.bmp","*.jpeg"},QDir::Files);check(files.size()==2,"only human original/export and scene images directly at root");
        QMap<QString,QByteArray> originals;for(const auto& name:files)originals[QDir(settings.outputDirectory).filePath(name)]=hash(QDir(settings.outputDirectory).filePath(name));
        for(const auto& result:output.batch.outputs){check(!result.error && result.status=="SAVED","all flat outputs saved");check(hash(QString::fromStdString(result.path)).toStdString()==result.sha256,"saved output digest");}
        SceneSources sources;core::CaptureProfile restored;check(loadHistoricalSources(manifest,sources,restored,error) && sources.at("r").image.pixelColor(3,3)==QColor(Qt::red),"flat history reopens exact lossless source in all formats");
        if(QString(format)=="JPEG")check(QString::fromStdString(output.batch.outputs.front().path).contains(".aitsnap/batches/") || std::any_of(output.batch.outputs.begin(),output.batch.outputs.end(),[](const auto& x){return x.kind=="ORIGINAL" && QString::fromStdString(x.path).contains("source-evidence");}),"JPEG retains lossless metadata source");
        ReportOptions report;report.targetMode="automatic";report.text.titleTemplate="{profile} {sequence}";
        {ReportQueue queue(temp.filePath(QString(format)+"-queue"),"NO_OFFICE_HELPER_EXPECTED");check(queue.enqueue(output,report,error),"flat current report queues");auto jobs=queue.jobs();check(QFileInfo(jobs.front()["target"].toObject()["documentPath"].toString()).absolutePath()==QDir(settings.outputDirectory).absolutePath(),"daily report lives at human output root");check(!queue.enqueueHistorical(manifest,report,error),"duplicate batch report rejected");}
        {ReportQueue queue(temp.filePath(QString(format)+"-history-queue"),"NO_OFFICE_HELPER_EXPECTED");check(queue.enqueueHistorical(manifest,report,error),"flat historical report queues from original frozen source");check(queue.jobs().size()==1,"one persisted historical job without running Office");}
        auto bad=object;bad["extensions"]=QJsonObject{{"storageLayout","flat"}};check(batchImageRoot(temp.filePath("batch.json"),bad).isEmpty(),"forged flat tag outside metadata hierarchy rejected");check(batchArtifactPath(manifest,object,QJsonObject{{"relativePath","../settings.json"}}).isEmpty(),"relative traversal refused");
        request.requestId="batch-two";auto repeat=captureToFiles(backend,settings,request,true);check(writeBatchManifest(repeat,error),"repeat manifest");check(repeat.directory!=output.directory,"same time sequence gets unique metadata batch");check(QDir(settings.outputDirectory).entryList({"*.png","*.bmp","*.jpeg"},QDir::Files).size()==4,"colliding image names numbered rather than overwritten");for(auto it=originals.cbegin();it!=originals.cend();++it)check(hash(it.key())==it.value(),"older flat images immutable across repeat capture");
    }
    settings.outputDirectory=temp.filePath("copy-only");auto copy=captureToFiles(backend,settings,request,false,true);check(copy.directory.isEmpty() && copy.imageDirectory.isEmpty() && !QDir(settings.outputDirectory).exists(),"copy creates neither flat images nor metadata directories");
    settings.imagePathMode="batch";settings.outputDirectory=temp.filePath("traditional");auto batch=captureToFiles(backend,settings,request,true);check(batch.imageDirectory.isEmpty() && QFileInfo(batch.directory).absolutePath()==settings.outputDirectory,"default batch layout retained");
    settings.outputDirectory=temp.filePath("PNG");settings.imagePathMode="flat";settings.format="PNG";settings.colorMode="oled";const auto config=temp.filePath("settings.json");auto old=settingsJson(settings);old["schemaVersion"]=5;old.remove("imagePathMode");old["appearance"]=QJsonObject{{"language","system"},{"colorMode","dark"}};const auto legacy=QJsonDocument(old).toJson();check(saveBytesNew(legacy,config,error),"legacy v5 fixture");Settings loaded;check(loadSettings(config,loaded,error) && loaded.imagePathMode=="batch","v5 defaults preserve batch behavior");check(saveSettings(config,settings,error),"save schema6");const auto backups=QDir(temp.path()).entryList({"settings.json.v5-preserved-*"},QDir::Files);check(backups.size()==1,"one v5 preservation backup");QFile backup(temp.filePath(backups.front()));check(backup.open(QIODevice::ReadOnly) && backup.readAll()==legacy,"v5 original byte preservation");check(loadSettings(config,loaded,error) && loaded.imagePathMode=="flat" && loaded.colorMode=="oled","schema6 choices roundtrip");
    installLanguage("en-US");applyColorMode("oled");
    {MainWindow window(nullptr,config,"-",{},false,[&]{return backend.displays();});auto* themes=window.findChild<QComboBox*>("appearanceColorMode");auto* paths=window.findChild<QComboBox*>("imagePathMode");check(paths && paths->currentData()=="flat","UI path mode restored");
        themes->setCurrentIndex(themes->findData("oled"));app.processEvents();check(activeColorMode()=="oled" && app.palette().color(QPalette::Window)==QColor("#000000") && app.palette().color(QPalette::Base)==QColor("#000000"),"OLED main and content backgrounds pure black");
        themes->setCurrentIndex(themes->findData("nasa-punk"));app.processEvents();check(activeColorMode()=="nasa-punk" && app.palette().color(QPalette::Window)==QColor("#d9d1c1") && app.palette().color(QPalette::Text).lightness()<80,"NASA Punk warm industrial palette");window.findChild<QPushButton*>("applySettings")->click();check(loadSettings(config,loaded,error) && loaded.colorMode=="nasa-punk" && loaded.imagePathMode=="flat","theme and path UI persist");
        window.findChild<QListWidget*>("workspaceNavigation")->setCurrentRow(3);app.processEvents();check(window.findChild<QListWidget*>("batchCards")->count()>=2,"gallery discovers flat hidden metadata batches");
        bool helpVerified=false;QTimer::singleShot(50,&window,[&]{auto* dialog=window.findChild<QDialog*>("reportTextHelpDialog");if(dialog){auto* text=dialog->findChild<QTextBrowser*>("reportTextHelpContent");helpVerified=text && text->toPlainText().contains("{scene_revision}") && text->toPlainText().contains("UTC");dialog->reject();}});window.findChild<QPushButton*>("reportTextHelp")->click();check(helpVerified,"offline help bundled and readable through app");
    }
    captureWhileSaving(temp.filePath("pipeline"), settings);
    reportDiagnosticPresentation(temp.filePath("word-diagnostics"), settings, batch);
    firstRunWithoutSampleSources(temp.filePath("first-run"), settings);
    reportRemovalPresentation(temp.filePath("queue-removal-ui"), settings, batch);
    std::cout<<"PRACTICAL_PASS flat/batch formats collision preservation history queue routing migration OLED NASA offline-help; real Word NOT_RUN\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
