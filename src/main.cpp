// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 AITSnap contributors

#include "main_window.h"
#include "compat/qt_widgets_compat.h"
#include "desktop/appearance.h"
#include "desktop/desktop_activation.h"

#include <QApplication>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QGuiApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QElapsedTimer>
#include <QEvent>
#include <windows.h>
#include <psapi.h>

static quint64 processCpuTime()
{
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0;
    ULARGE_INTEGER k{}, u{}; k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime; u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return k.QuadPart + u.QuadPart;
}

static QString modulePath(const QString& name)
{
    const auto native = name.toStdWString(); const auto module = GetModuleHandleW(native.c_str());
    if (!module) return {}; wchar_t path[32768]{}; const auto size = GetModuleFileNameW(module, path, 32768);
    return size ? QFileInfo(QString::fromWCharArray(path, int(size))).canonicalFilePath() : QString{};
}
static bool showExistingWindow(QLockFile& lock)
{
    qint64 pid{}; QString host, name;
    if (!lock.getLockInfo(&pid, &host, &name) || pid <= 0) return false;
    return aitsnap::activateDesktopProcess(DWORD(pid));
}

// Opt-in, append-only phase evidence. It records no configuration/document contents.
class StartupTrace final : public QObject {
public:
    QElapsedTimer clock;QFile file;
    StartupTrace(){clock.start();}
    bool open(const QString& path){file.setFileName(path);return file.open(QIODevice::WriteOnly|QIODevice::NewOnly);}
    void stage(const QString& name){if(!file.isOpen())return;const auto bytes=QJsonDocument(QJsonObject{{"stage",name},{"elapsedMs",clock.elapsed()},{"pid",qint64(GetCurrentProcessId())},{"version",AITSNAP_VERSION}}).toJson(QJsonDocument::Compact)+'\n';file.write(bytes);file.flush();}
    bool eventFilter(QObject* object,QEvent* event) override {
        if(event->type()==QEvent::Paint){stage("main-first-paint-dispatched");object->removeEventFilter(this);}return false;
    }
};
int main(int argc, char* argv[])
{
    StartupTrace trace;
    aitsnap::compat::configureHighDpi();
    QApplication app(argc, argv);
    auto font = app.font(); font.setPointSize(10); app.setFont(font);
    QApplication::setApplicationName(QStringLiteral("AITSnap"));
    QApplication::setApplicationVersion(QStringLiteral(AITSNAP_VERSION));
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addOption(QCommandLineOption("tray", "Start in tray"));
    parser.addOption(QCommandLineOption("show", "Open the main window, overriding the saved tray preference"));
    parser.addOption(QCommandLineOption("data-dir", "Explicit configuration/evidence directory", "directory"));
    parser.addOption(QCommandLineOption("startup-trace", "Write startup phase evidence to a new JSONL file (no content capture)", "new-jsonl"));
    parser.addOption(QCommandLineOption("smoke-report", "Hidden event-loop/dependency smoke; create a new JSON receipt", "new-json"));
    parser.addOption(QCommandLineOption("smoke-idle-ms", "Optional idle interval inside hidden smoke (1..60000 ms)", "milliseconds", "1"));
    parser.process(app);
    if(parser.isSet("startup-trace") && !trace.open(parser.value("startup-trace")))return 4;
    trace.stage("application-ready-arguments-parsed");
    bool durationOk{}; const auto idleMs = parser.value("smoke-idle-ms").toInt(&durationOk);
    if (!durationOk || idleMs < 1 || idleMs > 60000) return 2;
    const auto configuration = parser.isSet("data-dir") ? QDir(parser.value("data-dir")).absoluteFilePath("settings-v1.json") : aitsnap::settingsPath();
    if (!QDir().mkpath(QFileInfo(configuration).absolutePath())) return 3;
    aitsnap::Settings preferences = aitsnap::defaultSettings(); QString preferencesError;
    if (QFile::exists(configuration)) aitsnap::loadSettings(configuration, preferences, preferencesError);
    aitsnap::installLanguage(preferences.language); aitsnap::applyColorMode(preferences.colorMode);
    trace.stage(preferencesError.isEmpty()?"preferences-loaded":"preferences-invalid-preserved");
    QApplication::setWindowIcon(QIcon(":/brand/aitsnap.ico"));
    QLockFile lock(QFileInfo(configuration).absolutePath() + "/desktop.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        trace.stage("existing-instance-lock-held");
        if (parser.isSet("smoke-report")) return 2;
        if ((!parser.isSet("tray") || parser.isSet("show")) && showExistingWindow(lock)) return 0;
        if (parser.isSet("tray")) return 0;
        QMessageBox::information(nullptr, QStringLiteral("AITSnap"), aitsnap::uiText("AITSnap 已在运行。请从通知区域打开现有窗口。"));
        return 2;
    }
    trace.stage("single-instance-lock-acquired");
    QApplication::setQuitOnLastWindowClosed(false);

    aitsnap::MainWindow window(nullptr, configuration, parser.isSet("smoke-report") ? QStringLiteral("-") : QString{}, [&trace](const QString& stage){trace.stage(stage);});
    window.installEventFilter(&trace);
    window.winId(); // Keep an addressable target even while Qt has hidden the main window.
    aitsnap::DesktopActivation activation(window);
    trace.stage("main-native-handle-ready");
    QTimer::singleShot(0,&app,[&trace]{trace.stage("event-loop-first-turn");});
    quint64 cpuBefore{}; QElapsedTimer idleClock;
    if (parser.isSet("smoke-report")) {
        QTimer::singleShot(250, &app, [&] { cpuBefore = processCpuTime(); idleClock.start(); });
        QTimer::singleShot(250 + idleMs, &app, [&] {
            QJsonObject modules; for (const auto& suffix : {"Core", "Gui", "Widgets"}) { const auto name = QString("Qt%1%2.dll").arg(QT_VERSION_MAJOR).arg(suffix); modules[name] = modulePath(name); } modules["qwindows.dll"] = modulePath("qwindows.dll");
            QJsonObject report{{"status", "EVENT_LOOP_SMOKE_PASS"}, {"version", app.applicationVersion()}, {"qtRuntime", qVersion()}, {"qtCompiled", QT_VERSION_STR}, {"processBits", int(sizeof(void*) * 8)}, {"platform", app.platformName()}, {"screenCount", QGuiApplication::screens().size()}, {"executable", app.applicationFilePath()}, {"configuration", configuration}, {"modules", modules}, {"humanAcceptance", "HOLD"}};
            const auto cpuAfter = processCpuTime(); const auto elapsedMs = double(idleClock.nsecsElapsed()) / 1000000.0;
            if (cpuAfter >= cpuBefore && cpuBefore && elapsedMs > 0) { const auto cpuMs = double(cpuAfter - cpuBefore) / 10000.0; report["idle"] = QJsonObject{{"elapsedMs", elapsedMs}, {"processCpuMs", cpuMs}, {"oneCorePercent", cpuMs / elapsedMs * 100.0}, {"scope", "hidden idle MainWindow; no capture or Word jobs; not a 24h soak"}}; }
            PROCESS_MEMORY_COUNTERS memory{}; memory.cb = sizeof(memory); if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) { report["workingSetBytes"] = qint64(memory.WorkingSetSize); report["peakWorkingSetBytes"] = qint64(memory.PeakWorkingSetSize); }
            QFile file(parser.value("smoke-report")); const auto bytes = QJsonDocument(report).toJson();
            const bool saved = file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(bytes) == bytes.size(); file.close(); app.exit(saved ? 0 : 4);
        });
    } else if (parser.isSet("show") || (!window.startInTray() && !parser.isSet("tray"))) window.show();
    trace.stage("event-loop-enter");
    const auto result=app.exec();trace.stage("event-loop-exit");return result;
}
