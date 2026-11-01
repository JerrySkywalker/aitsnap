// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include "main_window.h"
#include "adapters/win_capture.h"
#include "compat/qt_widgets_compat.h"
#include "desktop/mini_capture_window.h"
#include "desktop/live_output_window.h"
#include "adapters/live_preview.h"
#include "core/scene_geometry.h"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QLineEdit>
#include <QClipboard>
#include <QComboBox>
#include <QListWidget>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static LRESULT CALLBACK paint(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps);
        RECT a{0, 0, 100, 200}, b{100, 0, 200, 200};
        auto red = CreateSolidBrush(RGB(255, 0, 0)), green = CreateSolidBrush(RGB(0, 255, 0));
        FillRect(dc, &a, red); FillRect(dc, &b, green); DeleteObject(red); DeleteObject(green); EndPaint(hwnd, &ps); return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
int main(int argc, char** argv)
{
    compat::configureHighDpi(); QApplication app(argc, argv); app.setApplicationName("AITSnapTests"); app.setQuitOnLastWindowClosed(false);
    std::vector<HWND> windows;
    try {
        QTemporaryDir temp; check(temp.isValid(), "temp directory"); QString error;
        Settings settings = defaultSettings(); settings.outputDirectory = temp.path();
        core::CaptureProfile profile; profile.id = "synthetic"; profile.name = "Synthetic";
        profile.regions.push_back({"one", "One", {0, 0, 16, 16}, true}); settings.profiles.push_back(profile);
        const auto path = temp.filePath("settings.json"); check(saveSettings(path, settings, error), "save settings");
        Settings loaded; check(loadSettings(path, loaded, error) && loaded.profiles[0].regions[0].rect == profile.regions[0].rect, "restart persistence");
        QFile corrupt(temp.filePath("future.json")); check(corrupt.open(QIODevice::WriteOnly), "future settings file"); corrupt.write("{\"schemaVersion\":99}"); corrupt.close();
        check(!loadSettings(corrupt.fileName(), loaded, error), "future version rejected"); error.clear();
        QImage source(16, 16, QImage::Format_ARGB32); source.fill(Qt::red);
        for (const auto& format : {"PNG", "JPEG", "BMP"}) {
            auto file = temp.filePath(QString("format.") + QString(format).toLower());
            check(saveImageNew(source, file, format, error), "encode format");
            check(QImage(file).size() == source.size(), "decode format dimensions");
            check(!saveImageNew(source, file, format, error), "never overwrite"); error.clear();
        }
        check(!saveImageNew(source, temp.filePath("missing/subdir/file.png"), "PNG", error), "disk path failure explicit"); error.clear();
        core::CaptureRequest request; request.profile = profile; request.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
        settings.filenameTemplate = "{region}_{unknown}"; check(expandFilename(settings, request, profile.regions[0], error).isEmpty() && !error.isEmpty(), "unknown filename token"); error.clear();
        settings.filenameTemplate = "../{region}"; check(!expandFilename(settings, request, profile.regions[0], error).contains('/'), "path traversal sanitized");
        {
            Hotkeys keys; app.installNativeEventFilter(&keys);
            check(keys.configure({"Ctrl+Alt+Shift+F13", "Ctrl+Alt+Shift+F14", "Ctrl+Alt+Shift+F15"}, error), "register hotkeys");
            check(!RegisterHotKey(nullptr, 0x4500, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F13), "hotkey collision reported by OS");
            check(keys.pause(true, error), "pause hotkeys");
            check(RegisterHotKey(nullptr, 0x4500, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F13), "paused key released");
            check(!keys.pause(false, error), "resume collision explicit"); UnregisterHotKey(nullptr, 0x4500); error.clear();
            check(keys.pause(false, error), "resume hotkeys");
            int triggered = -1; keys.triggered = [&](int mode) { triggered = mode; };
            PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY, 0x4150, 0);
            QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec();
            check(triggered == 0, "native hotkey event dispatch");
        }
        std::cout << "ADAPTER_PASS settings/migration, PNG/JPEG/BMP, no-overwrite, disk failure, filename, hotkey collision/pause/dispatch\n";
        if (app.arguments().contains("--check-lock-guard")) {
            const auto evidence = app.arguments().indexOf("--evidence");
            if (evidence >= 0 && evidence + 1 < app.arguments().size()) settings.outputDirectory = app.arguments()[evidence + 1];
            const auto status = interactiveDesktopStatus();
            if (status.code != "SESSION_LOCKED") { std::cout << "LOCK_GUARD_NOT_RUN expected locked test session\n"; return 2; }
            WinCaptureBackend backend; auto displays = backend.displays(); check(!displays.empty(), "lock guard display inventory");
            const auto frame = backend.acquire(displays.front()); check(frame.error.code == "SESSION_LOCKED" && !frame.image.valid() && frame.acquiredAtUtc.empty(), "locked session cannot create a captured frame");
            auto lockedProfile = profile; lockedProfile.calibratedDisplays = displays; lockedProfile.regions[0].rect = {displays.front().rect.x + 20, displays.front().rect.y + 20, 16, 16};
            core::CaptureRequest blocked; blocked.requestId = "locked-" + QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString(); blocked.profile = lockedProfile; blocked.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
            const auto outcome = captureToFiles(backend, settings, blocked, true); const auto json = batchJson(outcome.batch);
            check(json["status"].toString() == "FAILED" && outcome.batch.captures[0].error.code == "SESSION_LOCKED", "locked batch accurately failed");
            check(writeBatchManifest(outcome, error), "locked failure receipt");
            std::cout << "LOCK_GUARD_PASS SESSION_LOCKED no_frame no_false_acquisition manifest=" << outcome.directory.toStdString() << "/batch.json\n"; return 0;
        }
        if (app.arguments().contains("--native")) {
            if (const auto status = interactiveDesktopStatus(); status) { std::cout << "NATIVE_HOLD " << status.code << ' ' << status.message << '\n'; return 2; }
            WinCaptureBackend backend; profile.calibratedDisplays = backend.displays(); check(!profile.calibratedDisplays.empty(), "native displays");
            WNDCLASSW wc{}; wc.lpfnWndProc = paint; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AITSnapDesktopTest";
            check(RegisterClassW(&wc) != 0, "native pattern class");
            for (const auto& display : profile.calibratedDisplays) {
                HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"Synthetic AITSnap", WS_POPUP,
                    display.rect.x + 80, display.rect.y + 80, 200, 200, nullptr, nullptr, wc.hInstance, nullptr);
                check(window != nullptr, "native pattern window"); windows.push_back(window); ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window);
            }
            DwmFlush(); Sleep(150); profile.regions.clear();
            for (int i = 0; i < 5; ++i) {
                auto display = profile.calibratedDisplays[std::size_t(i) % profile.calibratedDisplays.size()];
                profile.regions.push_back({"r" + std::to_string(i), "Synthetic " + std::to_string(i), {display.rect.x + 90 + (i % 2) * 110, display.rect.y + 90 + i * 10, 16, 16}, true});
            }
            settings = defaultSettings(); settings.outputDirectory = temp.filePath("captures"); settings.profiles = {profile}; settings.clipboardRegion = "r1";
            auto secondary = profile; secondary.id = "secondary"; secondary.name = "Secondary"; secondary.regions.resize(1); settings.profiles.push_back(secondary);
            settings.saveHotkey = "Ctrl+Alt+Shift+F20"; settings.copyHotkey = "Ctrl+Alt+Shift+F21"; settings.bothHotkey = "Ctrl+Alt+Shift+F22";
            check(saveSettings(path, settings, error), "synthetic main settings");
            const auto evidence = app.arguments().indexOf("--evidence");
            if (evidence >= 0 && evidence + 1 < app.arguments().size()) { settings.outputDirectory = app.arguments()[evidence + 1]; check(saveSettings(path, settings, error), "evidence config"); }
            if(app.arguments().contains("--live")){settings.scenes={core::presetScene(profile,"live-scene","实时拼图",160,100,core::LayoutPreset::Grid)};check(saveSettings(path,settings,error),"live scene fixture");}
            MainWindow host(nullptr, path);int extraCaptures=0;
            if(app.arguments().contains("--live")){
                auto settle=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
                host.show();settle(1700);auto* live=dynamic_cast<LivePreviewService*>(host.findChild<QObject*>("livePreviewService"));check(live && live->active() && live->snapshot().sources.size()==5,"workspace samples before first evidence capture");
                Settings unchanged;check(loadSettings(path,unchanged,error) && unchanged.sequence==0,"live frames do not consume sequence");
                check(!QDir(settings.outputDirectory).exists() || QDir(settings.outputDirectory).entryList({"*Synthetic*"},QDir::Dirs|QDir::NoDotAndDotDot).empty(),"live frames create no batch directory");
                check(QDir(temp.filePath("report-jobs")).entryList({"*.json"},QDir::Files).empty(),"live frames enqueue no Word jobs");
                check(live->snapshot().sources.at("r0").image.pixelColor(4,4)==QColor(Qt::red),"workspace live pixels match known source");
                host.findChild<QPushButton*>("openLiveOutput")->click();settle(1500);auto* output=dynamic_cast<LiveOutputWindow*>(host.findChild<QWidget*>("liveOutputWindow"));check(output && output->isVisible(),"selected source opens independent live window");
                output->findChild<QAction*>("copyPreview")->trigger();check(QApplication::clipboard()->image().pixelColor(4,4)==QColor(Qt::red),"independent live window displays actual source pixels");
                std::shared_ptr<CaptureOutcome> fresh;QEventLoop freshWait;host.captureFinished=[&](auto result){fresh=result;freshWait.quit();};host.triggerCapture(1);check(!output->isVisible(),"formal capture hides live output windows");QTimer::singleShot(10000,&freshWait,&QEventLoop::quit);freshWait.exec();check(fresh && fresh->directory.isEmpty() && output->isVisible() && toQImage(fresh->batch.captures.front().image).pixelColor(4,4)==QColor(Qt::red),"formal capture uses fresh correct pixels and restores output");extraCaptures=1;
                output->findChild<QPushButton*>("liveOutputPause")->click();check(output->paused(),"output can pause");output->close();
                host.findChild<QListWidget*>("workspaceNavigation")->setCurrentRow(2);host.findChild<QPushButton*>("openLiveOutput")->click();settle(1500);output=dynamic_cast<LiveOutputWindow*>(host.findChild<QWidget*>("liveOutputWindow"));check(output && output->key()=="scene:live-scene","scene page opens the selected scene rather than stale workbench selection");output->findChild<QAction*>("copyPreview")->trigger();const auto expected=renderScenePreview(settings.scenes.front(),live->snapshot().sources,{1600,1000});check(expected.complete && QApplication::clipboard()->image()==expected.image,"independent scene uses exact shared live composition algorithm");output->close();
                host.findChild<QCheckBox*>("livePreviewEnabled")->setChecked(false);host.hide();settle(150);check(!live->active() && live->snapshot().sources.empty(),"last subscription stops live capture and releases cache");
                std::cout<<"LIVE_WORKSPACE_NATIVE_PASS before-first-capture no-sequence no-disk no-Word exact-pixels independent-window pause release\n";
            }
            QEventLoop wait;
            std::shared_ptr<CaptureOutcome> outcome;
            host.captureFinished = [&](auto result) { outcome = result; wait.quit(); };
            const auto foreground = GetForegroundWindow();
            QTimer::singleShot(0, &host, [&] { PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY, 0x4152, 0); });
            QTimer::singleShot(10000, &wait, &QEventLoop::quit); wait.exec();
            check(bool(outcome), "capture completion timeout");
            check(GetForegroundWindow() == foreground && !host.isVisible(), "background focus preserved");
            check(outcome->batch.captures.size() == 5, "five region batch");
            int saved = 0, copied = 0;
            for (const auto& capture : outcome->batch.captures) {
                check(!capture.error && capture.image.valid(), "native region acquired");
                const auto image = toQImage(capture.image); const int i = std::stoi(capture.region.id.substr(1));
                if (image.pixelColor(4, 4) != QColor(i % 2 ? Qt::green : Qt::red)) std::cerr << "PIXEL_MISMATCH region=" << capture.region.id << " argb=" << image.pixelColor(4, 4).name(QColor::HexArgb).toStdString() << " expected=" << (i % 2 ? "green" : "red") << '\n';
                check(image.pixelColor(4, 4) == QColor(i % 2 ? Qt::green : Qt::red), "native exact region pixel");
            }
            for (const auto& output : outcome->batch.outputs) {
                if (output.error) std::cerr << output.error.code << ' ' << output.error.message << '\n';
                check(!output.error, "output failure"); if (output.status == "SAVED") ++saved; if (output.status == "COPIED") ++copied;
            }
            check(saved == 5 && copied == 1, "five files and one clipboard");
            check(QApplication::clipboard()->image().pixelColor(4, 4) == QColor(Qt::green), "selected clipboard image");
            check(QFile::exists(QDir(outcome->directory).filePath("batch.json")), "batch manifest saved");
            Settings restarted; check(loadSettings(path, restarted, error) && restarted.sequence == 1+extraCaptures, "persistent capture sequence");
            std::cout << "NATIVE_M1_AUTOMATED_PASS displays=" << profile.calibratedDisplays.size() << " regions=5 saved=5 clipboard=1 focus=preserved elapsed_ms=" << outcome->elapsedMs << " manifest=" << outcome->directory.toStdString() << "/batch.json\n";
            if(app.arguments().contains("--capture-contract")) {
                const char* buttons[]{"captureSave","captureCopy","captureBoth"};
                for(int mode=0;mode<3;++mode){outcome.reset();const auto destination=temp.filePath(QString("mode-%1").arg(mode));
                    host.findChild<QLineEdit*>("outputDirectory")->setText(destination);host.findChild<QCheckBox*>("reportEnabled")->setChecked(mode==1);
                    QEventLoop actionWait;host.captureFinished=[&](auto result){outcome=result;actionWait.quit();};
                    QTimer::singleShot(0,&host,[&]{host.findChild<QPushButton*>(buttons[mode])->click();});QTimer::singleShot(10000,&actionWait,&QEventLoop::quit);actionWait.exec();
                    check(bool(outcome),"shared capture action completed");int fileCount=0,copyCount=0;for(const auto& output:outcome->batch.outputs){check(!output.error,"shared action output error");fileCount+=output.status=="SAVED";copyCount+=output.status=="COPIED";}
                    check(fileCount==(mode==1?0:5) && copyCount==(mode==0?0:1),"save/copy/both side effects match named actions");
                    check(mode!=1 || (outcome->directory.isEmpty() && !QDir(destination).exists()),"fresh copy never writes an image batch");
                    check(QDir(temp.filePath("report-jobs")).entryList({"*.json"},QDir::Files).empty(),"copy-only mode creates no Word job even when reporting enabled");
                    Settings captured;check(loadSettings(path,captured,error) && captured.sequence==mode+2+extraCaptures && captured.outputDirectory==destination,"every UI action commits the current draft and advances exactly one sequence");
                }
                std::cout<<"CAPTURE_ACTION_CONTRACT_PASS save/copy/both fresh side effects draft routing exact sequence no copy Word job\n";
            }
            if(app.arguments().contains("--mini")){
                host.findChild<QPushButton*>("openMiniWindow")->click();
                auto* mini=dynamic_cast<MiniCaptureWindow*>(host.findChild<QWidget*>("miniCaptureWindow"));check(mini && mini->isVisible() && !host.isVisible(),"mini mode entered");
                const auto area=profile.regions.front().rect;mini->move(area.x,area.y);QApplication::processEvents();DwmFlush();
                mini->findChild<QComboBox*>("miniCaptureMode")->setCurrentIndex(1);
                outcome.reset();QEventLoop miniWait;host.captureFinished=[&](auto result){outcome=result;miniWait.quit();};
                mini->findChild<QPushButton*>("miniCaptureButton")->click();check(!mini->isVisible(),"mini hides before asynchronous capture");
                QTimer::singleShot(10000,&miniWait,&QEventLoop::quit);miniWait.exec();check(bool(outcome) && mini->isVisible(),"mini restored after capture");
                check(outcome->directory.isEmpty() && outcome->batch.captures.front().image.valid() && toQImage(outcome->batch.captures.front().image).pixelColor(4,4)==QColor(Qt::red),"mini does not obscure source pixels or change copy-only semantics");
                auto* pause=mini->findChild<QPushButton*>("miniPause");pause->click();check(!mini->findChild<QPushButton*>("miniCaptureButton")->isEnabled(),"mini pause shares capture gate");pause->click();check(mini->findChild<QPushButton*>("miniCaptureButton")->isEnabled(),"mini resume shares capture gate");
                mini->findChild<QPushButton*>("miniOpenMain")->click();check(host.isVisible() && !mini->isVisible(),"mini returns to actual main window");host.hide();
                std::cout<<"MINI_NATIVE_PASS shared-copy hidden-before-acquire exact-source-pixel restored pause resume main-return\n";
            }
            if (app.arguments().contains("--editor")) {
                const auto before = outcome;
                auto* edit = host.findChild<QPushButton*>("editRegions"); check(edit != nullptr, "editor entry");
                edit->click();
                QEventLoop editing; QTimer::singleShot(500, &editing, &QEventLoop::quit); editing.exec();
                std::vector<QWidget*> overlays;
                for (auto* widget : QApplication::topLevelWidgets()) if (widget->isVisible() && widget->windowTitle().startsWith(QStringLiteral("AITSnap · 屏幕区域编辑"))) overlays.push_back(widget);
                check(overlays.size() == profile.calibratedDisplays.size(), "per-display editor windows");
                host.triggerCapture(0); // Must reject while overlays exist, not acquire editor pixels.
                QKeyEvent cancel(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); QApplication::sendEvent(overlays.front(), &cancel);
                QEventLoop closing; QTimer::singleShot(450, &closing, &QEventLoop::quit); closing.exec();
                check(outcome == before, "overlay capture request rejected");
                Settings cancelled; check(loadSettings(path, cancelled, error), "cancelled settings readable");
                check(cancelled.profiles[0].regions.size() == profile.regions.size() && cancelled.profiles[0].regions[0].rect == profile.regions[0].rect, "cancel preserves physical regions");
                for (auto* widget : QApplication::topLevelWidgets()) check(!widget->isVisible() || !widget->windowTitle().startsWith(QStringLiteral("AITSnap · 屏幕区域编辑")), "overlays removed before capture resumes");
                edit->click(); QEventLoop accepting; QTimer::singleShot(500, &accepting, &QEventLoop::quit); accepting.exec();
                overlays.clear(); for (auto* widget : QApplication::topLevelWidgets()) if (widget->isVisible() && widget->windowTitle().startsWith(QStringLiteral("AITSnap · 屏幕区域编辑"))) overlays.push_back(widget);
                check(!overlays.empty(), "second editor session");
                host.findChild<QComboBox*>("profiles")->setCurrentIndex(1);
                QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(overlays.front(), &accept);
                QEventLoop accepted; QTimer::singleShot(450, &accepted, &QEventLoop::quit); accepted.exec();
                Settings switched; check(loadSettings(path, switched, error) && switched.activeProfile == 1 && switched.profiles[0].id == profile.id && switched.profiles[0].regions.size() == 5 && switched.profiles[1].id == "secondary" && switched.profiles[1].regions.size() == 1, "editor commits to stable profile id despite active switch");
                std::cout << "REGION_EDITOR_NATIVE_PASS windows=" << overlays.size() << " capture_guard=PASS cancel_roundtrip=PASS stable_profile_accept=PASS\n";
            }
            if (app.arguments().contains("--benchmark")) {
                std::vector<double> elapsed, skews;
                core::CaptureRequest bench = outcome->batch.request;
                const auto run = QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString();
                for (int i = 0; i < 100; ++i) {
                    bench.requestId = "benchmark-" + run + "-" + std::to_string(i);
                    bench.issuedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
                    auto result = captureToFiles(backend, settings, bench, true);
                    for (const auto& output : result.batch.outputs) check(!output.error && output.status == "SAVED", "benchmark output");
                    for (const auto& capture : result.batch.captures) check(!capture.error, "benchmark capture");
                    elapsed.push_back(result.elapsedMs);
                    if (!result.batch.displayFrames.empty()) {
                        auto first = result.batch.displayFrames.front().acquisitionStartNs, last = first;
                        for (const auto& frame : result.batch.displayFrames) { first = std::min(first, frame.acquisitionStartNs); last = std::max(last, frame.acquisitionStartNs); }
                        skews.push_back(double(last - first) / 1000000.0);
                    }
                }
                std::sort(elapsed.begin(), elapsed.end()); std::sort(skews.begin(), skews.end());
                std::cout << "BASELINE steady_clock n=100 regions=5 size=16x16 physical_displays=" << profile.calibratedDisplays.size()
                          << " format=PNG capture_save_p50_ms=" << elapsed[49] << " p95_ms=" << elapsed[94]
                          << " display_start_skew_p50_ms=" << skews[49] << " p95_ms=" << skews[94] << '\n';
            }
        }
        for (auto window : windows) DestroyWindow(window);
    } catch (const std::exception& e) {
        for (auto window : windows) DestroyWindow(window);
        std::cerr << "FAIL " << e.what() << '\n'; return 1;
    }
}
