// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "main_window.h"
#include "adapters/win_capture.h"
#include "compat/qt_widgets_compat.h"
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static QByteArray read(const QString& path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "read evidence"); return f.readAll(); }
static bool await(const std::function<bool()>& condition, int timeout = 10000) { const auto end = QDateTime::currentMSecsSinceEpoch() + timeout; while (!condition() && QDateTime::currentMSecsSinceEpoch() < end) { QCoreApplication::processEvents(); QThread::msleep(20); } return condition(); }
static QJsonObject cli(const QString& pipe, QStringList args, int code = 0)
{
    args.append({"--pipe-name", pipe}); QProcess process; QEventLoop loop; QTimer timer; timer.setSingleShot(true);
    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit); QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    process.start(QCoreApplication::applicationDirPath() + "/aitsnap-cli.exe", args); timer.start(20000); loop.exec();
    if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(1000); throw std::runtime_error("CLI synthetic host timeout"); }
    const auto output = process.readAllStandardOutput(); if (process.exitCode() != code) std::cerr << output.toStdString() << process.readAllStandardError().toStdString();
    check(process.exitCode() == code, "CLI exit disposition"); return QJsonDocument::fromJson(output).object();
}
static LRESULT CALLBACK paint(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_PAINT) { PAINTSTRUCT ps{}; const auto dc = BeginPaint(window, &ps); RECT left{0, 0, 100, 200}, right{100, 0, 200, 200}; auto red = CreateSolidBrush(RGB(255, 0, 0)), green = CreateSolidBrush(RGB(0, 255, 0)); FillRect(dc, &left, red); FillRect(dc, &right, green); DeleteObject(red); DeleteObject(green); EndPaint(window, &ps); return 0; } return DefWindowProcW(window, message, w, l);
}
int main(int argc, char** argv)
{
    compat::configureHighDpi(); QApplication app(argc, argv); app.setApplicationName("AITSnapIpcTests"); app.setQuitOnLastWindowClosed(false); std::vector<HWND> windows;
    try {
        check(app.arguments().size() >= 2, "fixture directory");
        if (const auto status = interactiveDesktopStatus(); status) { std::cout << "NATIVE_HOLD " << status.code << '\n'; return 2; }
        QTemporaryDir temporary; QString root = temporary.path(); if (app.arguments().size() >= 3) root = QDir(app.arguments()[2]).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); check(QDir().mkpath(root), "test output");
        WinCaptureBackend backend; core::CaptureProfile profile; profile.id = "m4-profile"; profile.name = "Public synthetic native M4"; profile.calibratedDisplays = backend.displays(); check(!profile.calibratedDisplays.empty(), "native displays");
        WNDCLASSW wc{}; wc.lpfnWndProc = paint; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AITSnapIpcPattern"; check(RegisterClassW(&wc) != 0, "pattern class");
        for (const auto& display : profile.calibratedDisplays) { const auto window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"Synthetic public M4", WS_POPUP, display.rect.x + 80, display.rect.y + 80, 200, 200, nullptr, nullptr, wc.hInstance, nullptr); check(window != nullptr, "pattern window"); windows.push_back(window); ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window); }
        DwmFlush(); QThread::msleep(150);
        for (int i = 0; i < 5; ++i) { const auto d = profile.calibratedDisplays[std::size_t(i) % profile.calibratedDisplays.size()]; profile.regions.push_back({"r" + std::to_string(i), "Synthetic " + std::to_string(i), {d.rect.x + 90 + (i % 2) * 110, d.rect.y + 90 + i * 10, 16, 16}, true}); }
        core::SceneTemplate scene; scene.id = "m4-row"; scene.name = "Synthetic row"; scene.width = 128; scene.height = 64; scene.layers = {{"red", "r0"}, {"green", "r1"}}; scene.layers[0].destination = {0, 0, 64, 64, 0}; scene.layers[1].destination = {64, 0, 64, 64, 0}; scene.layers[1].zIndex = 1;
        Settings settings = defaultSettings(); settings.outputDirectory = root + "/captures"; settings.profiles = {profile}; settings.scenes = {scene}; settings.selectedSceneIds = QStringList{"m4-row"}; settings.outputMode = "both"; settings.clipboardRegion = "scene:m4-row"; settings.saveHotkey = "Ctrl+Alt+Shift+F20"; settings.copyHotkey = "Ctrl+Alt+Shift+F21"; settings.bothHotkey = "Ctrl+Alt+Shift+F22"; settings.report.enabled = true; settings.report.targetMode = "specified"; settings.report.documentPath = root + "/synthetic-event-report.docx";
        auto other = profile; other.id = "other-profile"; other.name = "Other"; settings.profiles.push_back(other); QString error; const auto path = root + "/settings.json"; check(saveSettings(path, settings, error), "isolated host settings");
        const QJsonObject rule{{"enabled", true}, {"sourceSystem", "SYNTHETIC_AIT_EXECUTOR"}, {"eventType", "COMMAND_ACKNOWLEDGED"}, {"stepId", "STEP-002"}, {"commandDefinitionId", "CMD-READ-V"}, {"profileId", "m4-profile"}, {"sceneIds", QJsonArray{"m4-row"}}, {"mode", "both"}};
        check(saveBytesNew(QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"rules", QJsonArray{rule}}}).toJson(), root + "/host-rules-v1.json", error), "persisted event mapping");
        const auto pipe = "AITSnap.m4-" + QUuid::createUuid().toString(QUuid::WithoutBraces); const auto fixtures = QDir(app.arguments()[1]).absolutePath(); QString firstId, secondId;
        {
            MainWindow host(nullptr, path, pipe); std::vector<std::shared_ptr<CaptureOutcome>> outcomes; host.captureFinished = [&](auto result) { outcomes.push_back(result); };
            check(cli(pipe, {"list-profiles"})["result"].toObject()["profiles"].toArray().size() == 2, "external profile enumeration");
            cli(pipe, {"select-profile", "--profile-id", "other-profile", "--key", "select-other"}); cli(pipe, {"select-profile", "--profile-id", "m4-profile", "--key", "select-m4"});
            cli(pipe, {"import-catalog", "--file", fixtures + "/sequence-catalog.json", "--key", "catalog"});
            const auto first = cli(pipe, {"submit-event", "--file", fixtures + "/execution-event.json", "--key", "event-one"}); firstId = first["result"].toObject()["requestId"].toString(); check(!firstId.isEmpty(), "rule mapped event to capture request");
            check(await([&] { return outcomes.size() == 1; }), "first native event capture");
            auto replay = cli(pipe, {"submit-event", "--file", fixtures + "/execution-event.json", "--key", "event-one"}); check(replay["replayed"].toBool() && replay["result"].toObject()["requestId"] == firstId, "accepted request key replay");
            check(cli(pipe, {"submit-event", "--file", fixtures + "/execution-event.json", "--key", "different-transport-key"})["status"] == "DUPLICATE", "duplicate event with new key no capture");
            const auto conflict = cli(pipe, {"submit-event", "--file", fixtures + "/execution-event.json", "--key", "event-one", "--mode", "save"}, 1); check(conflict["error"].toObject()["code"] == "KEY_CONFLICT", "no changed request under accepted key");
            check(outcomes.size() == 1 && QDir(settings.outputDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() == 1, "no duplicate screenshot directory");
            auto retry = QJsonDocument::fromJson(read(fixtures + "/execution-event.json")).object(); retry["eventId"] = "EVT-SYN-M4-RETRY"; retry["sourceSequenceNo"] = 1000; retry["sourceTimeUtc"] = "2026-10-31T06:00:00.000Z"; auto sequence = retry["sequence"].toObject(); sequence["runId"] = "RUN-SYN-001"; retry["sequence"] = sequence; auto command = retry["command"].toObject(); command["attempt"] = 3; command["invocationId"] = "INV-SYN-M4-003"; retry["command"] = command;
            check(saveBytesNew(QJsonDocument(retry).toJson(), root + "/retry-event.json", error), "synthetic out-of-order retry input");
            secondId = cli(pipe, {"submit-event", "--file", root + "/retry-event.json", "--key", "event-two"})["result"].toObject()["requestId"].toString(); check(!secondId.isEmpty() && secondId != firstId && await([&] { return outcomes.size() == 2; }), "second run/retry maps independently");
            for (int batch = 0; batch < 2; ++batch) {
                const auto& outcome = outcomes[batch]; check(outcome->batch.captures.size() == 5 && outcome->composites.size() == 1, "one native batch five sources plus scene"); int saved = 0, copied = 0;
                for (const auto& output : outcome->batch.outputs) { check(!output.error, "native event output success"); if (output.status == "SAVED") ++saved; if (output.status == "COPIED") ++copied; }
                check(saved == 6 && copied == 1, "rule output save and copy"); for (int i = 0; i < 5; ++i) check(toQImage(outcome->batch.captures[std::size_t(i)].image).pixelColor(4, 4) == QColor(i % 2 ? Qt::green : Qt::red), "native event exact source pixels");
                const auto& context = outcome->batch.request.context; check(context.attempt == (batch ? 3 : 2) && context.sequenceRunId == (batch ? "RUN-SYN-001" : "RUN-SYN-002"), "run and attempt frozen");
                check(QFile::exists(outcome->directory + "/batch.json"), "native event manifest exists");
            }
            const auto image = QApplication::clipboard()->image(); check(image.size() == QSize(128, 64) && image.pixelColor(16, 16) == QColor(Qt::red) && image.pixelColor(96, 16) == QColor(Qt::green), "native composite selected clipboard");
            check(await([&] { int committed = 0; for (const auto& file : QDir(root + "/report-jobs").entryList({"*.json"}, QDir::Files)) { if (file.contains(".result.")) continue; const auto job = QJsonDocument::fromJson(read(root + "/report-jobs/" + file)).object(); if (job["state"] == "COMMITTED") ++committed; } return committed == 2; }, 60000), "event to two real Word committed jobs");
            for (const auto& outcome : outcomes) for (const auto& output : outcome->batch.outputs) if (output.kind == "ORIGINAL") check(QCryptographicHash::hash(read(QString::fromStdString(output.path)), QCryptographicHash::Sha256).toHex().toStdString() == output.sha256, "Word leaves originals immutable");
        }
        {
            MainWindow restarted(nullptr, path, pipe); int newCaptures = 0; restarted.captureFinished = [&](auto) { ++newCaptures; };
            const auto replay = cli(pipe, {"submit-event", "--file", fixtures + "/execution-event.json", "--key", "event-one"}); check(replay["replayed"].toBool() && replay["result"].toObject()["requestId"] == firstId && newCaptures == 0, "desktop restart does not repeat accepted event capture");
        }
        auto shutdown = settings; shutdown.outputDirectory = root + "/shutdown/captures"; shutdown.report.documentPath = root + "/shutdown/report.docx"; const auto shutdownPath = root + "/shutdown/settings.json";
        check(saveSettings(shutdownPath, shutdown, error), "shutdown source settings");
        { MainWindow closing(nullptr, shutdownPath, "-"); closing.triggerCapture(0); } // no event loop: destructor must drain completion safely
        const auto savedOnExit = QDir(shutdown.outputDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot); check(savedOnExit.size() == 1, "active capture saved on exit");
        const auto exitBatch = QJsonDocument::fromJson(read(shutdown.outputDirectory + "/" + savedOnExit[0] + "/batch.json")).object(); check(exitBatch["status"] == "CAPTURED" && exitBatch["extensions"].toObject()["contextCorrelationStatus"] == "UNLINKED", "exit drains manifest with explicit unlinked context");
        const auto queuedOnExit = QDir(root + "/shutdown/report-jobs").entryList({"*.json"}, QDir::Files); check(queuedOnExit.size() == 1, "exit preserves one independent report job"); const auto queued = QJsonDocument::fromJson(read(root + "/shutdown/report-jobs/" + queuedOnExit[0])).object(); check(queued["state"] == "QUEUED" && queued["attemptCount"] == 0, "exit does not wait for Word or lose queued intent");
        const QJsonObject receipt{{"status", "PASS"}, {"nativeDisplays", int(profile.calibratedDisplays.size())}, {"batches", QJsonArray{firstId, secondId}}, {"sourcesPerBatch", 5}, {"scenesPerBatch", 1}, {"wordCommittedJobs", 2}, {"duplicateCaptureCount", 0}, {"shutdownBatchAndQueuedJob", true}, {"document", settings.report.documentPath}};
        check(saveBytesNew(QJsonDocument(receipt).toJson(), root + "/receipt.json", error), "M4 receipt"); for (const auto window : windows) DestroyWindow(window);
        std::cout << "IPC_NATIVE_M4_PASS " << QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString() << " evidence=" << root.toStdString() << '\n';
    } catch (const std::exception& error) { for (const auto window : windows) DestroyWindow(window); std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
