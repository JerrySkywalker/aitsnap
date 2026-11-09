// SPDX-License-Identifier: Apache-2.0
#include "adapters/capture_files.h"
#include "adapters/batch_paths.h"
#include "adapters/report_queue.h"
#include "core/scene_geometry.h"
#include "desktop/appearance.h"
#include "main_window.h"
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListWidget>
#include <QPushButton>
#include <QSemaphore>
#include <QTemporaryDir>
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
        const auto& file = outcome.batch.outputs.front();
        check(!file.error && file.status == "SAVED" &&
              QImage(QString::fromStdString(file.path)).pixelColor(3, 3) == expected[i],
              "saved batch contains the pixels captured at its own trigger");
    }
    std::cout << "PIPELINE_PASS 2s blocked output, exact next frames, bounded admission, FIFO receipts\n";
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
    std::cout<<"PRACTICAL_PASS flat/batch formats collision preservation history queue routing migration OLED NASA offline-help; real Word NOT_RUN\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
